// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Severity enum based on LogMessage format. Needs to stay in sync with the
 * MultideviceInternalsLogsHandler.
 * @enum {number}
 */
export const Severity = {
  VERBOSE: -1,
  INFO: 0,
  WARNING: 1,
  ERROR: 2,
};

/**
 * The type of log message object. The definition is based on
 * chrome/browser/ui/webui/ash/multidevice_internals/multidevice_internals_logs_handler.cc:
 * LogMessageToDictionary()
 * @typedef {{text: string,
 *            time: string,
 *            file: string,
 *            line: number,
 *            severity: Severity}}
 */
export let LogMessage;

/**
 * Numerical values should not be changed because they must stay in sync with
 * chromeos/ash/components/phonehub/feature_status.h.
 * @enum{number}
 */
export const FeatureStatus = {
  NOT_ELIGIBLE_FOR_FEATURE: 0,
  ELIGIBLE_PHONE_BUT_NOT_SETUP: 1,
  PHONE_SELECTED_AND_PENDING_SETUP: 2,
  DISABLED: 3,
  UNAVAILABLE_BLUETOOTH_OFF: 4,
  ENABLED_BUT_DISCONNECTED: 5,
  ENABLED_AND_CONNECTING: 6,
  ENABLED_AND_CONNECTED: 7,
};

/**
 * Numerical values should not be changed because they must stay in sync with
 * MobileStatus in chromeos/ash/components/phonehub/phone_status_model.h.
 * @enum{number}
 */
export const MobileStatus = {
  NO_SIM: 0,
  SIM_BUT_NO_RECEPTION: 1,
  SIM_WITH_RECEPTION: 2,
};

/**
 * Numerical values should not be changed because they must stay in sync with
 * SignalStrength in chromeos/ash/components/phonehub/phone_status_model.h.
 * @enum{number}
 */
export const SignalStrength = {
  ZERO_BARS: 0,
  ONE_BAR: 1,
  TWO_BARS: 2,
  THREE_BARS: 3,
  FOUR_BARS: 4,
};

/**
 * Numerical values should not be changed because they must stay in sync with
 * ChargingState in chromeos/ash/components/phonehub/phone_status_model.h.
 * @enum{number}
 */
export const ChargingState = {
  NOT_CHARGING: 0,
  CHARGING_AC: 1,
  CHARGING_USB: 2,
};

/**
 * Numerical values should not be changed because they must stay in sync with
 * BatterySaverState in chromeos/ash/components/phonehub/phone_status_model.h.
 * @enum{number}
 */
export const BatterySaverState = {
  OFF: 0,
  ON: 1,
};

/**
 * With the exception of NONE, numerical values should not be changed because
 * they must stay in sync with ImageType in
 * chromeos/multidevice_internals/multidevice_internals_phone_hub_handler.cc.
 * @enum{number}
 */
export const ImageType = {
  NONE: 0,
  PINK: 1,
  RED: 2,
  GREEN: 3,
  BLUE: 4,
  YELLOW: 5,
};

/**
 * Maps a ImageType to its title label in the dropdown.
 * @type {!Map<ImageType, String>}
 */
export const imageTypeToStringMap = new Map([
  [ImageType.NONE, 'None'],
  [ImageType.PINK, 'Pink'],
  [ImageType.RED, 'Red'],
  [ImageType.GREEN, 'Green'],
  [ImageType.BLUE, 'Blue'],
  [ImageType.YELLOW, 'Yellow'],
]);

/**
 * @typedef {{
 *   mobileStatus: !MobileStatus,
 *   signalStrength: !SignalStrength,
 *   mobileProvider: string,
 *   chargingState: !ChargingState,
 *   batterySaverState: !BatterySaverState,
 *   batteryPercentage: number,
 * }}
 */
export let PhoneStatusModel;

/**
 * @typedef {{
 *   isValid: boolean,
 *   url: string,
 *   title: string,
 *   lastAccessedTimeStamp: number,
 *   favicon: !ImageType,
 * }}
 */
export let BrowserTabsMetadataModel;

/**
 * @typedef {{
 *   isTabSyncEnabled: boolean,
 *   browserTabOneMetadata: ?BrowserTabsMetadataModel,
 *   browserTabTwoMetadata: ?BrowserTabsMetadataModel
 * }}
 */
export let BrowserTabsModel;

/**
 * Numerical values should not be changed because they must stay in sync with
 * Importance in chromeos/ash/components/phonehub/notification.h.
 * @enum{number}
 */
export const Importance = {
  UNSPECIFIED: 0,
  NONE: 1,
  MIN: 2,
  LOW: 3,
  DEFAULT: 4,
  HIGH: 5,
};

/**
 * Maps an Importance to its title label in the dropdown.
 * @type {!Map<Importance, String>}
 */
export const importanceToString = new Map([
  [Importance.UNSPECIFIED, 'Unspecified'],
  [Importance.NONE, 'None'],
  [Importance.MIN, 'Min'],
  [Importance.LOW, 'Low'],
  [Importance.DEFAULT, 'Default'],
  [Importance.HIGH, 'High'],
]);

/**
 * @typedef {{
 *   visibleAppName: string,
 *   packageName: string,
 *   icon: !ImageType,
 * }}
 */
export let AppMetadata;

/**
 * With the exception of the sent property, values match with Notifications in
 * chromeos/ash/components/phonehub/notification.h.
 * @typedef {{
 *   sent: boolean,
 *   id: number,
 *   appMetadata: !AppMetadata,
 *   timestamp: number,
 *   importance: !Importance,
 *   inlineReplyId: number,
 *   title: ?string,
 *   textContent: ?string,
 *   sharedImage: !ImageType,
 *   contactImage: !ImageType,
 * }}
 */
export let Notification;

/**
 * Numerical values should not be changed because they must stay in sync with
 * TetherController::Status in
 * chromeos/ash/components/phonehub/tether_controller.h.
 * @enum{number}
 */
export const TetherStatus = {
  INELIGIBLE_FOR_FEATURE: 0,
  CONNETION_UNAVAILABLE: 1,
  CONNECTION_AVAILABLE: 2,
  CONNECTING: 3,
  CONNECTED: 4,
  NO_RECEPTION: 5,
};

/**
 * Maps an TetherStatus to its title label in the dropdown.
 * @type {!Map<TetherStatus, String>}
 */
export const tetherStatusToString = new Map([
  [TetherStatus.INELIGIBLE_FOR_FEATURE, 'Ineligible for feature'],
  [TetherStatus.CONNETION_UNAVAILABLE, 'Connection unavailable'],
  [TetherStatus.CONNECTION_AVAILABLE, 'Connection available'],
  [TetherStatus.CONNECTING, 'Connecting'],
  [TetherStatus.CONNECTED, 'Connected'],
  [TetherStatus.NO_RECEPTION, 'No reception'],
]);

/**
 * Numerical values should not be changed because they must stay in sync with
 * FindMyDeviceController::Status (TBA) in
 * chromeos/ash/components/phonehub/find_my_device_controller.h.
 * @enum{number}
 */
export const FindMyDeviceStatus = {
  NOT_AVAILABLE: 0,
  OFF: 1,
  ON: 2,
};

/**
 * Maps an FindMyDeviceStatus to its title label in the dropdown.
 * @type {!Map<FindMyDeviceStatus, String>}
 */
export const findMyDeviceStatusToString = new Map([
  [FindMyDeviceStatus.NOT_AVAILABLE, 'Not Available'],
  [FindMyDeviceStatus.OFF, 'Off'],
  [FindMyDeviceStatus.ON, 'On'],
]);

/**
 * @enum{number}
 */
export const FileType = {
  IMAGE: 0,
  VIDEO: 1,
};

/**
 * @enum{number}
 */
export const DownloadResult = {
  SUCCESS: 0,
  ERROR_GENERIC: 1,
  ERROR_STORAGE: 2,
  ERROR_NETWORK: 3,
};

/**
 * @typedef {{
 *   isCameraRollEnabled: boolean,
 *   isOnboardingDismissed: boolean,
 *   isFileAccessGranted: boolean,
 *   isLoadingViewShown: boolean,
 *   numberOfThumbnails: number,
 *   fileType: !FileType,
 *   downloadResult: !DownloadResult,
 * }}
 */
export let CameraRollManager;
