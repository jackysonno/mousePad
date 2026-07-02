#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h> 
#include <unistd.h>  

#include "utils.h"
#include "pad.h"
#include "user.h"
#include "config.h"

attr_public const char *g_pluginName = "remote_gamepad";
attr_public const char *g_pluginDesc = "Control your ps4 through network";
attr_public const char *g_pluginAuth = "xfangfang";
attr_public uint32_t g_pluginVersion = 0x00000120; // 1.2.0

#define PLUGIN_CONFIG_PATH GOLDHEN_PATH "/remote_pad.ini"
#define PLUGIN_DEFAULT_SECTION "default"

static RemoteUserService *remoteUserService;
static RemotePadService *remotePad;

static bool prxLoaded = false;
static bool g_systemReady = false; 

// Variables to capture and hijack the main physical controller
static int32_t real_main_handle = -1;
static int32_t virtual_main_handle = -1;

HOOK_DEFINE(scePadInit, void) {
    if (remotePad->initDriver() != SCE_OK) {
        final_printf("Failed to init remote pad driver\n");
    }
    final_printf("RemotePad driver loaded\n");
    return HOOK_PASS(scePadInit);
}

HOOK_DEFINE(scePadOpen, int32_t userId, int32_t type, int32_t index, void *param) {
    // Get the real handle from the system first
    int32_t handle = HOOK_PASS(scePadOpen, userId, type, index, param);
    
    // Capture the very first active hardware controller handle used by the game
    if (handle >= 0 && real_main_handle == -1) {
        real_main_handle = handle;
        // Map this user to pad0 (index 0) inside the remote pad system
        virtual_main_handle = remotePad->open(userId, type, 0, param);
        final_printf("RemotePad: Successfully linked Real Handle %d to Virtual Pad 0\n", real_main_handle);
    }
    return handle;
}

HOOK_DEFINE(scePadClose, int32_t handle) {
    if (g_systemReady && handle == real_main_handle && virtual_main_handle != -1) {
        remotePad->close(virtual_main_handle);
        real_main_handle = -1;
        virtual_main_handle = -1;
    }
    return HOOK_PASS(scePadClose, handle);
}

HOOK_DEFINE(scePadGetHandle, int32_t userId, uint32_t controller_type, uint32_t controller_index) {
    return HOOK_PASS(scePadGetHandle, userId, controller_type, controller_index);
}

HOOK_DEFINE(scePadReadState, int32_t handle, OrbisPadData *data) {
    // If 90 seconds passed and the game reads the main controller, feed it phone data!
    if (g_systemReady && handle == real_main_handle && virtual_main_handle != -1) {
        if (remotePad->readState(virtual_main_handle, data) == SCE_OK)
            return SCE_OK;
    }
    return HOOK_PASS(scePadReadState, handle, data);
}

HOOK_DEFINE(scePadRead, int32_t handle, OrbisPadData *data, int32_t count) {
    // If 90 seconds passed and the game reads the main controller, feed it phone data!
    if (g_systemReady && handle == real_main_handle && virtual_main_handle != -1) {
        int32_t ret = remotePad->read(virtual_main_handle, data, count);
        if (ret >= 0)
            return ret;
    }
    return HOOK_PASS(scePadRead, handle, data, count);
}

HOOK_DEFINE(scePadGetControllerInformation, int32_t handle, OrbisPadInformation *info) {
    if (g_systemReady && handle == real_main_handle && virtual_main_handle != -1) {
        if (remotePad->getControllerInformation(virtual_main_handle, info) == SCE_OK)
            return SCE_OK;
    }
    return HOOK_PASS(scePadGetControllerInformation, handle, info);
}

HOOK_DEFINE(scePadSetLightBar, int32_t handle, OrbisPadColor *inputColor) {
    if (g_systemReady && handle == real_main_handle && virtual_main_handle != -1) {
        if (remotePad->setLightBar(virtual_main_handle, inputColor) == SCE_OK)
            return SCE_OK;
    }
    return HOOK_PASS(scePadSetLightBar, handle, inputColor);
}

HOOK_DEFINE(scePadResetLightBar, int32_t handle) {
    if (g_systemReady && handle == real_main_handle && virtual_main_handle != -1) {
        if (remotePad->resetLightBar(virtual_main_handle) == SCE_OK)
            return SCE_OK;
    }
    return HOOK_PASS(scePadResetLightBar, handle);
}

HOOK_DEFINE(scePadSetVibration, int32_t handle, const OrbisPadVibeParam *param) {
    if (g_systemReady && handle == real_main_handle && virtual_main_handle != -1) {
        if (remotePad->setVibration(virtual_main_handle, param) == SCE_OK)
            return SCE_OK;
    }
    return HOOK_PASS(scePadSetVibration, handle, param);
}

HOOK_DEFINE(scePadResetOrientation, int32_t handle) {
    if (g_systemReady && handle == real_main_handle && virtual_main_handle != -1) {
        if (remotePad->resetOrientation(virtual_main_handle) == SCE_OK)
            return SCE_OK;
    }
    return HOOK_PASS(scePadResetOrientation, handle);
}

HOOK_DEFINE(scePadSetMotionSensorState, int32_t handle, bool enable) {
    if (g_systemReady && handle == real_main_handle && virtual_main_handle != -1) {
        if (remotePad->setMotionSensorState(virtual_main_handle, enable) == SCE_OK)
            return SCE_OK;
    }
    return HOOK_PASS(scePadSetMotionSensorState, handle, enable);
}

HOOK_DEFINE(scePadSetTiltCorrectionState, int32_t handle, bool enable) {
    if (g_systemReady && handle == real_main_handle && virtual_main_handle != -1) {
        if (remotePad->setTiltCorrectionState(virtual_main_handle, enable) == SCE_OK)
            return SCE_OK;
    }
    return HOOK_PASS(scePadSetTiltCorrectionState, handle, enable);
}

HOOK_DEFINE(scePadSetAngularVelocityDeadbandState, int32_t handle, bool enable) {
    if (g_systemReady && handle == real_main_handle && virtual_main_handle != -1) {
        if (remotePad->setAngularVelocityDeadbandState(virtual_main_handle, enable) == SCE_OK)
            return SCE_OK;
    }
    return HOOK_PASS(scePadSetAngularVelocityDeadbandState, handle, enable);
}

HOOK_DEFINE(scePadDeviceClassParseData, int32_t handle, const OrbisPadData *data, OrbisPadDeviceClassData *classData) {
    if (g_systemReady && handle == real_main_handle && virtual_main_handle != -1) {
        if (remotePad->deviceClassParseData(virtual_main_handle, data, classData) == SCE_OK)
            return SCE_OK;
    }
    return HOOK_PASS(scePadDeviceClassParseData, handle, data, classData);
}

HOOK_DEFINE(scePadDeviceClassGetExtendedInformation, int32_t handle, OrbisPadDeviceClassExtInfo *info) {
    if (g_systemReady && handle == real_main_handle && virtual_main_handle != -1) {
        if (remotePad->deviceClassGetExtInfo(virtual_main_handle, info) == SCE_OK)
            return SCE_OK;
    }
    return HOOK_PASS(scePadDeviceClassGetExtendedInformation, handle, info);
}

// Passthrough user service hooks to avoid freezing the loading screen
HOOK_DEFINE(sceUserServiceGetLoginUserIdList, OrbisUserServiceLoginUserIdList *list) {
    return HOOK_PASS(sceUserServiceGetLoginUserIdList, list);
}

HOOK_DEFINE(sceUserServiceGetEvent, OrbisUserServiceEvent *event) {
    return HOOK_PASS(sceUserServiceGetEvent, event);
}

HOOK_DEFINE(sceUserServiceGetUserName, int32_t userId, char *username, size_t size) {
    return HOOK_PASS(sceUserServiceGetUserName, userId, username, size);
}

HOOK_DEFINE(sceUserServiceGetUserColor, int32_t userId, OrbisUserServiceUserColor *color) {
    return HOOK_PASS(sceUserServiceGetUserColor, userId, color);
}

int32_t load_config(ini_table_s *table, const char *section_name) {
    return 0; // Config skipping since we are forcing direct hardware hijacking
}

void *delayed_activation_thread(void *arg) {
    sleep(90); // Wait 90 seconds for GTA V loading to fully complete
    g_systemReady = true;
    final_printf("RemotePad: Safe mode deactivated. Hardware controller hijacked successfully!\n");
    Notify(TEX_ICON_SYSTEM, "RemotePad: Connected to Pad0!"); 
    return NULL;
}

int32_t attr_public plugin_load(int32_t argc, const char *argv[]) {
    final_printf("[GoldHEN] %s Plugin Started.\n", g_pluginName);
    
    if (load_prx("libScePad.sprx", true))
        return -1;

    if (load_prx("libSceUserService.sprx", false))
        return -1;

    prxLoaded = true;

    remoteUserService = initRemoteUserService();
    remotePad = initRemotePadService();
    if (!remotePad) return -1;

    // Apply all hooks immediately at second zero
    HOOK32(scePadInit);
    HOOK32(scePadOpen);
    HOOK32(scePadGetHandle);
    HOOK32(scePadRead);
    HOOK32(scePadReadState);
    HOOK32(scePadGetControllerInformation);
    HOOK32(scePadSetLightBar);
    HOOK32(scePadResetLightBar);
    HOOK32(scePadSetVibration);
    HOOK32(scePadResetOrientation);
    HOOK32(scePadSetMotionSensorState);
    HOOK32(scePadSetTiltCorrectionState);
    HOOK32(scePadSetAngularVelocityDeadbandState);
    HOOK32(scePadDeviceClassParseData);
    HOOK32(scePadDeviceClassGetExtendedInformation);
    HOOK32(scePadClose);

    HOOK32(sceUserServiceGetLoginUserIdList);
    HOOK32(sceUserServiceGetUserColor);
    HOOK32(sceUserServiceGetUserName);
    HOOK32(sceUserServiceGetEvent);

    pthread_t activation_tid;
    pthread_create(&activation_tid, NULL, delayed_activation_thread, NULL);
    pthread_detach(activation_tid);

    return 0;
}

int32_t attr_public plugin_unload(int32_t argc, const char *argv[]) {
    if (!prxLoaded) return 0;

    UNHOOK(scePadInit);
    UNHOOK(scePadOpen);
    UNHOOK(scePadGetHandle);
    UNHOOK(scePadRead);
    UNHOOK(scePadReadState);
    UNHOOK(scePadGetControllerInformation);
    UNHOOK(scePadSetLightBar);
    UNHOOK(scePadResetLightBar);
    UNHOOK(scePadSetVibration);
    UNHOOK(scePadResetOrientation);
    UNHOOK(scePadSetMotionSensorState);
    UNHOOK(scePadSetTiltCorrectionState);
    UNHOOK(scePadSetAngularVelocityDeadbandState);
    UNHOOK(scePadDeviceClassParseData);
    UNHOOK(scePadDeviceClassGetExtendedInformation);
    UNHOOK(scePadClose);

    UNHOOK(sceUserServiceGetLoginUserIdList);
    UNHOOK(sceUserServiceGetUserColor);
    UNHOOK(sceUserServiceGetUserName);
    UNHOOK(sceUserServiceGetEvent);
    return 0;
}

int32_t attr_module_hidden module_start(int64_t argc, const void *args) { return 0; }
int32_t attr_module_hidden module_stop(int64_t argc, const void *args) { return 0; }
