// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';


/**
 * Enumeration for device state about remaining space.
 * These values must be kept in sync with
 * StorageManagerHandler::StorageSpaceState in C++ code.
 */
export enum StorageSpaceState {
  NORMAL = 0,
  LOW = 1,
  CRITICALLY_LOW = 2,
}

type SystemDisplayApi = typeof chrome.system.display;

let systemDisplayApi: SystemDisplayApi|null = null;

export function setDisplayApiForTesting(testDisplayApi: SystemDisplayApi):
    void {
  systemDisplayApi = testDisplayApi;
}

export function getDisplayApi(): SystemDisplayApi {
  if (!systemDisplayApi) {
    systemDisplayApi = chrome.system.display;
  }
  return systemDisplayApi;
}

export interface PowerSource {
  id: string;
  is_dedicated_charger: boolean;
  description: string;
}

export interface BatteryStatus {
  present: boolean;
  charging: boolean;
  calculating: boolean;
  percent: number;
  statusText: string;
}

/**
 * Mirrors ash::settings::PowerHandler::IdleBehavior.
 */
export enum IdleBehavior {
  DISPLAY_OFF_SLEEP = 0,
  DISPLAY_OFF = 1,
  DISPLAY_ON = 2,
  SHUT_DOWN = 3,
  STOP_SESSION = 4,
}

/**
 * Mirrors chromeos::PowerPolicyController::Action.
 */
export enum LidClosedBehavior {
  SUSPEND = 0,
  STOP_SESSION = 1,
  SHUT_DOWN = 2,
  DO_NOTHING = 3,
}

export interface PowerManagementSettings {
  possibleAcIdleBehaviors: IdleBehavior[];
  possibleBatteryIdleBehaviors: IdleBehavior[];
  acIdleManaged: boolean;
  batteryIdleManaged: boolean;
  currentAcIdleBehavior: IdleBehavior;
  currentBatteryIdleBehavior: IdleBehavior;
  lidClosedBehavior: LidClosedBehavior;
  lidClosedControlled: boolean;
  hasLid: boolean;
  adaptiveCharging: boolean;
  adaptiveChargingManaged: boolean;
  batterySaverFeatureEnabled: boolean;
}

/**
 * A note app's availability for running as note handler app from lock screen.
 * Mirrors `ash::LockScreenAppSupport`.
 */
export enum NoteAppLockScreenSupport {
  NOT_SUPPORTED = 0,
  NOT_ALLOWED_BY_POLICY = 1,
  SUPPORTED = 2,
  ENABLED = 3,
}

export interface NoteAppInfo {
  name: string;
  value: string;
  preferred: boolean;
  lockScreenSupport: NoteAppLockScreenSupport;
}

export interface ExternalStorage {
  label: string;
  uuid: string;
}

export interface DevicePageBrowserProxy {
  /** Initializes the mouse and touchpad handler. */
  initializePointers(): void;

  /** Initializes the stylus handler. */
  initializeStylus(): void;

  /** Initializes the keyboard WebUI handler. */
  initializeKeyboard(): void;

  /** Initializes the keyboard update watcher. */
  initializeKeyboardWatcher(): void;

  /** Shows the Ash shortcut customization app. */
  showShortcutCustomizationApp(): void;

  /** Requests an ARC status update. */
  updateAndroidEnabled(): void;

  /** Requests a power status update. */
  updatePowerStatus(): void;

  /**
   * Sets the ID of the power source to use.
   * @param powerSourceId ID of the power source. '' denotes the
   *     battery (no external power source).
   */
  setPowerSource(powerSourceId: string): void;

  /** Requests the current power management settings. */
  requestPowerManagementSettings(): void;

  /**
   * Sets the idle power management behavior.
   */
  setIdleBehavior(behavior: IdleBehavior, whenOnAc: boolean): void;

  /**
   * Sets the lid-closed power management behavior.
   */
  setLidClosedBehavior(behavior: LidClosedBehavior): void;

  /**
   * Sets adaptive charging on or off.
   */
  setAdaptiveCharging(enabled: boolean): void;

  /**
   * |callback| is run when there is new note-taking app information
   * available or after |requestNoteTakingApps| has been called.
   */
  setNoteTakingAppsUpdatedCallback(
      callback: (apps: NoteAppInfo[], waitingForAndroid: boolean) => void):
      void;

  /**
   * Open up the play store with the given URL.
   */
  showPlayStore(url: string): void;

  /**
   * Request current note-taking app info. Invokes any callback registered in
   * |onNoteTakingAppsUpdated|.
   */
  requestNoteTakingApps(): void;

  /**
   * Changes the preferred note taking app.
   * @param appId The app id. This should be a value retrieved from a
   *     |onNoteTakingAppsUpdated| callback.
   */
  setPreferredNoteTakingApp(appId: string): void;

  /**
   * Sets whether the preferred note taking app should be enabled to run as a
   * lock screen note action handler.
   */
  setPreferredNoteTakingAppEnabledOnLockScreen(enabled: boolean): void;

  /** Requests an external storage list update. */
  updateExternalStorages(): void;

  /**
   * |callback| is run when the list of plugged-in external storages is
   * available after |updateExternalStorages| has been called.
   */
  setExternalStoragesUpdatedCallback(
      callback: (storages: ExternalStorage[]) => void): void;

  /**
   * Sets |id| of display to render identification highlight on. Invalid |id|
   * turns identification highlight off. Handles any invalid input string as
   * invalid id.
   */
  highlightDisplay(id: string): void;

  /**
   * Updates the position of the dragged display to render preview indicators
   * as the display is being dragged around.
   */
  dragDisplayDelta(displayId: string, deltaX: number, deltaY: number): void;

  updateStorageInfo(): void;

  getStorageEncryptionInfo(): Promise<string>;

  openMyFiles(): void;

  openBrowsingDataSettings(): void;
}

let instance: DevicePageBrowserProxy|null = null;

export class DevicePageBrowserProxyImpl implements DevicePageBrowserProxy {
  static getInstance(): DevicePageBrowserProxy {
    return instance || (instance = new DevicePageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: DevicePageBrowserProxy): void {
    instance = obj;
  }

  initializePointers(): void {
    chrome.send('initializePointerSettings');
  }

  initializeStylus(): void {
    chrome.send('initializeStylusSettings');
  }

  initializeKeyboard(): void {
    chrome.send('initializeKeyboardSettings');
  }

  showShortcutCustomizationApp(): void {
    chrome.send('showShortcutCustomizationApp');
  }

  initializeKeyboardWatcher(): void {
    chrome.send('initializeKeyboardWatcher');
  }

  updateAndroidEnabled(): void {
    chrome.send('updateAndroidEnabled');
  }

  updatePowerStatus(): void {
    chrome.send('updatePowerStatus');
  }

  setPowerSource(powerSourceId: string): void {
    chrome.send('setPowerSource', [powerSourceId]);
  }

  requestPowerManagementSettings(): void {
    chrome.send('requestPowerManagementSettings');
  }

  setIdleBehavior(behavior: IdleBehavior, whenOnAc: boolean): void {
    chrome.send('setIdleBehavior', [behavior, whenOnAc]);
  }

  setAdaptiveCharging(enabled: boolean): void {
    chrome.send('setAdaptiveCharging', [enabled]);
  }

  setLidClosedBehavior(behavior: LidClosedBehavior): void {
    chrome.send('setLidClosedBehavior', [behavior]);
  }

  setNoteTakingAppsUpdatedCallback(
      callback: (apps: NoteAppInfo[], waitingForAndroid: boolean) => void):
      void {
    addWebUiListener('onNoteTakingAppsUpdated', callback);
  }

  showPlayStore(url: string): void {
    chrome.send('showPlayStoreApps', [url]);
  }

  requestNoteTakingApps(): void {
    chrome.send('requestNoteTakingApps');
  }

  setPreferredNoteTakingApp(appId: string): void {
    chrome.send('setPreferredNoteTakingApp', [appId]);
  }

  setPreferredNoteTakingAppEnabledOnLockScreen(enabled: boolean): void {
    chrome.send('setPreferredNoteTakingAppEnabledOnLockScreen', [enabled]);
  }

  updateExternalStorages(): void {
    chrome.send('updateExternalStorages');
  }

  setExternalStoragesUpdatedCallback(
      callback: (storages: ExternalStorage[]) => void): void {
    addWebUiListener('onExternalStoragesUpdated', callback);
  }

  highlightDisplay(id: string): void {
    chrome.send('highlightDisplay', [id]);
  }

  dragDisplayDelta(displayId: string, deltaX: number, deltaY: number): void {
    chrome.send('dragDisplayDelta', [displayId, deltaX, deltaY]);
  }

  updateStorageInfo(): void {
    chrome.send('updateStorageInfo');
  }

  getStorageEncryptionInfo(): Promise<string> {
    return sendWithPromise('getStorageEncryptionInfo');
  }

  openMyFiles(): void {
    chrome.send('openMyFiles');
  }

  openBrowsingDataSettings(): void {
    chrome.send('openBrowsingDataSettings');
  }
}
