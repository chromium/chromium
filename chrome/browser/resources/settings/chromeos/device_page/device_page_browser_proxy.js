// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {WebUIListener, addWebUIListener, addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format on

cr.define('settings', function() {
  /**
   * Enumeration for device state about remaining space.
   * These values must be kept in sync with
   * StorageManagerHandler::StorageSpaceState in C++ code.
   * @enum {number}
   */
  /* #export */ const StorageSpaceState = {
    NORMAL: 0,
    LOW: 1,
    CRITICALLY_LOW: 2
  };

  let systemDisplayApi = null;

  /* #export */ function setDisplayApiForTesting(testDisplayApi) {
    systemDisplayApi = testDisplayApi;
  }

  /* #export */ function getDisplayApi() {
    if (!systemDisplayApi) {
      systemDisplayApi = chrome.system.display;
    }
    return systemDisplayApi;
  }

  /**
   * @typedef {{
   *   id: string,
   *   is_dedicated_charger: boolean,
   *   description: string
   * }}
   */
  /* #export */ let PowerSource;

  /**
   * @typedef {{
   *   present: boolean,
   *   charging: boolean,
   *   calculating: boolean,
   *   percent: number,
   *   statusText: string,
   * }}
   */
  /* #export */ let BatteryStatus;

  /**
   * Mirrors chromeos::settings::PowerHandler::IdleBehavior.
   * @enum {number}
   */
  /* #export */ const IdleBehavior = {
    DISPLAY_OFF_SLEEP: 0,
    DISPLAY_OFF: 1,
    DISPLAY_ON: 2,
    SHUT_DOWN: 3,
    STOP_SESSION: 4,
  };

  /**
   * Mirrors chromeos::PowerPolicyController::Action.
   * @enum {number}
   */
  /* #export */ const LidClosedBehavior = {
    SUSPEND: 0,
    STOP_SESSION: 1,
    SHUT_DOWN: 2,
    DO_NOTHING: 3,
  };

  /**
   * @typedef {{
   *   possibleAcIdleBehaviors: !Array<settings.IdleBehavior>,
   *   possibleBatteryIdleBehaviors: !Array<settings.IdleBehavior>,
   *   acIdleManaged: boolean,
   *   batteryIdleManaged: boolean,
   *   currentAcIdleBehavior: settings.IdleBehavior,
   *   currentBatteryIdleBehavior: settings.IdleBehavior,
   *   lidClosedBehavior: settings.LidClosedBehavior,
   *   lidClosedControlled: boolean,
   *   hasLid: boolean,
   * }}
   */
  /* #export */ let PowerManagementSettings;

  /**
   * A note app's availability for running as note handler app from lock screen.
   * Mirrors chromeos::NoteTakingLockScreenSupport.
   * @enum {number}
   */
  /* #export */ const NoteAppLockScreenSupport =
      {NOT_SUPPORTED: 0, NOT_ALLOWED_BY_POLICY: 1, SUPPORTED: 2, ENABLED: 3};

  /**
   * @typedef {{
   *   name:string,
   *   value:string,
   *   preferred:boolean,
   *   lockScreenSupport: settings.NoteAppLockScreenSupport,
   * }}
   */
  /* #export */ let NoteAppInfo;

  /**
   * @typedef {{
   *   label: string,
   *   uuid: string
   * }}
   */
  /* #export */ let ExternalStorage;

  /** @interface */
  /* #export */ class DevicePageBrowserProxy {
    /** Initializes the mouse and touchpad handler. */
    initializePointers() {}

    /** Initializes the stylus handler. */
    initializeStylus() {}

    /** Initializes the keyboard WebUI handler. */
    initializeKeyboard() {}

    /** Initializes the keyboard update watcher. */
    initializeKeyboardWatcher() {}

    /** Shows the Ash keyboard shortcut viewer. */
    showKeyboardShortcutViewer() {}

    /** Requests an ARC status update. */
    updateAndroidEnabled() {}

    /** Requests a power status update. */
    updatePowerStatus() {}

    /**
     * Sets the ID of the power source to use.
     * @param {string} powerSourceId ID of the power source. '' denotes the
     *     battery (no external power source).
     */
    setPowerSource(powerSourceId) {}

    /** Requests the current power management settings. */
    requestPowerManagementSettings() {}

    /**
     * Sets the idle power management behavior.
     * @param {settings.IdleBehavior} behavior Idle behavior.
     * @param {boolean} whenOnAc If true sets AC idle behavior. Otherwise sets
     *     battery idle behavior.
     */
    setIdleBehavior(behavior, whenOnAc) {}

    /**
     * Sets the lid-closed power management behavior.
     * @param {settings.LidClosedBehavior} behavior Lid-closed behavior.
     */
    setLidClosedBehavior(behavior) {}

    /**
     * |callback| is run when there is new note-taking app information
     * available or after |requestNoteTakingApps| has been called.
     * @param {function(Array<settings.NoteAppInfo>, boolean):void} callback
     */
    setNoteTakingAppsUpdatedCallback(callback) {}

    /**
     * Open up the play store with the given URL.
     * @param {string} url
     */
    showPlayStore(url) {}

    /**
     * Request current note-taking app info. Invokes any callback registered in
     * |onNoteTakingAppsUpdated|.
     */
    requestNoteTakingApps() {}

    /**
     * Changes the preferred note taking app.
     * @param {string} appId The app id. This should be a value retrieved from a
     *     |onNoteTakingAppsUpdated| callback.
     */
    setPreferredNoteTakingApp(appId) {}

    /**
     * Sets whether the preferred note taking app should be enabled to run as a
     * lock screen note action handler.
     * @param {boolean} enabled Whether the app should be enabled to handle note
     *     actions from the lock screen.
     */
    setPreferredNoteTakingAppEnabledOnLockScreen(enabled) {}

    /** Requests an external storage list update. */
    updateExternalStorages() {}

    /**
     * |callback| is run when the list of plugged-in external storages is
     * available after |updateExternalStorages| has been called.
     * @param {function(Array<!settings.ExternalStorage>):void} callback
     */
    setExternalStoragesUpdatedCallback(callback) {}

    /**
     * Sets |id| of display to render identification highlight on. Invalid |id|
     * turns identification highlight off. Handles any invalid input string as
     * invalid id.
     * @param {string} id Display id of selected display.
     */
    highlightDisplay(id) {}

    /**
     * Updates the position of the dragged display to render preview indicators
     * as the display is being dragged around.
     * @param {string} id Display id of selected display.
     * @param {number} deltaX x-axis position change since the last update.
     * @param {number} deltaY y-axis position change since the last update.
     */
    dragDisplayDelta(id, deltaX, deltaY) {}

    updateStorageInfo() {}
    openMyFiles() {}
  }

  /**
   * @implements {settings.DevicePageBrowserProxy}
   */
  /* #export */ class DevicePageBrowserProxyImpl {
    /** @override */
    initializePointers() {
      chrome.send('initializePointerSettings');
    }

    /** @override */
    initializeStylus() {
      chrome.send('initializeStylusSettings');
    }

    /** @override */
    initializeKeyboard() {
      chrome.send('initializeKeyboardSettings');
    }

    /** @override */
    showKeyboardShortcutViewer() {
      chrome.send('showKeyboardShortcutViewer');
    }

    /** @override */
    initializeKeyboardWatcher() {
      chrome.send('initializeKeyboardWatcher');
    }

    /** @override */
    updateAndroidEnabled() {
      chrome.send('updateAndroidEnabled');
    }

    /** @override */
    updatePowerStatus() {
      chrome.send('updatePowerStatus');
    }

    /** @override */
    setPowerSource(powerSourceId) {
      chrome.send('setPowerSource', [powerSourceId]);
    }

    /** @override */
    requestPowerManagementSettings() {
      chrome.send('requestPowerManagementSettings');
    }

    /** @override */
    setIdleBehavior(behavior, whenOnAc) {
      chrome.send('setIdleBehavior', [behavior, whenOnAc]);
    }

    /** @override */
    setLidClosedBehavior(behavior) {
      chrome.send('setLidClosedBehavior', [behavior]);
    }

    /** @override */
    setNoteTakingAppsUpdatedCallback(callback) {
      cr.addWebUIListener('onNoteTakingAppsUpdated', callback);
    }

    /** @override */
    showPlayStore(url) {
      chrome.send('showPlayStoreApps', [url]);
    }

    /** @override */
    requestNoteTakingApps() {
      chrome.send('requestNoteTakingApps');
    }

    /** @override */
    setPreferredNoteTakingApp(appId) {
      chrome.send('setPreferredNoteTakingApp', [appId]);
    }

    /** @override */
    setPreferredNoteTakingAppEnabledOnLockScreen(enabled) {
      chrome.send('setPreferredNoteTakingAppEnabledOnLockScreen', [enabled]);
    }

    /** @override */
    updateExternalStorages() {
      chrome.send('updateExternalStorages');
    }

    /** @override */
    setExternalStoragesUpdatedCallback(callback) {
      cr.addWebUIListener('onExternalStoragesUpdated', callback);
    }

    /** @override */
    highlightDisplay(id) {
      chrome.send('highlightDisplay', [id]);
    }

    /** @override */
    dragDisplayDelta(id, deltaX, deltaY) {
      chrome.send('dragDisplayDelta', [id, deltaX, deltaY]);
    }

    /** @override */
    updateStorageInfo() {
      chrome.send('updateStorageInfo');
    }

    /** @override */
    openMyFiles() {
      chrome.send('openMyFiles');
    }
  }

  cr.addSingletonGetter(DevicePageBrowserProxyImpl);

  // #cr_define_end
  return {
    BatteryStatus,
    DevicePageBrowserProxy,
    DevicePageBrowserProxyImpl,
    ExternalStorage,
    IdleBehavior,
    LidClosedBehavior,
    NoteAppInfo,
    NoteAppLockScreenSupport,
    PowerManagementSettings,
    PowerSource,
    setDisplayApiForTesting,
    getDisplayApi,
    StorageSpaceState,
  };
});
