// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The possible statuses of hosts on the logged in account that determine the
 * page content. Note that this is based on (and must include an analog of
 * all values in) the HostStatus enum in
 * services/multidevice_setup/public/mojom/multidevice_setup.mojom.
 */
export enum MultiDeviceSettingsMode {
  NO_ELIGIBLE_HOSTS = 0,
  NO_HOST_SET = 1,
  HOST_SET_WAITING_FOR_SERVER = 2,
  HOST_SET_WAITING_FOR_VERIFICATION = 3,
  HOST_SET_VERIFIED = 4,
}

/**
 * Enum of MultiDevice features. Note that this is copied from (and must
 * include an analog of all values in) the Feature enum in
 * //chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.
 */
export enum MultiDeviceFeature {
  BETTER_TOGETHER_SUITE = 0,
  INSTANT_TETHERING = 1,
  // MESSAGES (2) is deprecated.
  SMART_LOCK = 3,
  PHONE_HUB = 4,
  PHONE_HUB_NOTIFICATIONS = 5,
  PHONE_HUB_TASK_CONTINUATION = 6,
  WIFI_SYNC = 7,
  ECHE = 8,
  PHONE_HUB_CAMERA_ROLL = 9,
}

/**
 * Possible states of MultiDevice features. Note that this is copied from (and
 * must include an analog of all values in) the FeatureState enum in
 * //chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.
 */
export enum MultiDeviceFeatureState {
  PROHIBITED_BY_POLICY = 0,
  DISABLED_BY_USER = 1,
  ENABLED_BY_USER = 2,
  NOT_SUPPORTED_BY_CHROMEBOOK = 3,
  NOT_SUPPORTED_BY_PHONE = 4,
  // UNAVAILABLE_NO_VERIFIED_HOST (5) is deprecated.
  UNAVAILABLE_INSUFFICIENT_SECURITY = 6,
  UNAVAILABLE_SUITE_DISABLED = 7,
  FURTHER_SETUP_REQUIRED = 8,
  UNAVAILABLE_TOP_LEVEL_FEATURE_DISABLED = 9,
  UNAVAILABLE_NO_VERIFIED_HOST_CLIENT_NOT_READY = 10,
  UNAVAILABLE_NO_VERIFIED_HOST_NO_ELIGIBLE_HOST = 11,
  UNAVAILABLE_NO_VERIFIED_HOST_HOST_EXISTS_BUT_NOT_SET_AND_VERIFIED = 12,
}

/**
 * Possible states of Phone Hub's feature access. Access can be
 * prohibited if the user is using a work profile on their phone on Android
 * version <N, or if the policy managing the phone disables access.
 */
export enum PhoneHubFeatureAccessStatus {
  PROHIBITED = 0,
  AVAILABLE_BUT_NOT_GRANTED = 1,
  ACCESS_GRANTED = 2,
}

/**
 * Possible reasons for Phone Hub's feature access being prohibited.
 * Users should ensure feature access is actually prohibited before
 * comparing against these reasons.
 */
export enum PhoneHubFeatureAccessProhibitedReason {
  UNKNOWN = 0,
  WORK_PROFILE = 1,
  DISABLED_BY_PHONE_POLICY = 2,
}

/**
 * Possible of Phone Hub's permissions setup mode．The value will be assigned
 * when the user clicks on the settings UI. Basically, INIT_MODE will be
 * default value, which means it has not been set yet.
 * ALL_PERMISSIONS_SETUP_MODE means that we will process notifications and
 * apps streaming onboarding flow in order.
 */
export enum PhoneHubPermissionsSetupMode {
  INIT_MODE = 0,
  NOTIFICATION_SETUP_MODE = 1,
  APPS_SETUP_MODE = 2,
  CAMERA_ROLL_SETUP_MODE = 3,
  ALL_PERMISSIONS_SETUP_MODE = 4,
}

/**
 * Numerical values the screens for combined set up dialog only.
 * Update of this enum should be propagate to PermissionsOnboardingStep
 * in chromeos/ash/components/phonehub/util/histogram_util.h.
 */
export enum PhoneHubPermissionsSetupFlowScreens {
  NOT_APPLICABLE = 0,
  INTRO = 1,
  FINISH_SET_UP_ON_PHONE = 2,
  CONNECTING = 3,
  CONNECTION_ERROR = 4,
  CONNECTION_TIME_OUT = 5,
  CONNECTED = 6,
  SET_A_PIN_OR_PASSWORD = 7,
}

/**
 * Numerical values the screens for actions in combined set up dialog only.
 * Update of this enum should be propagate to PermissionsOnboardingScreenEvent
 * in chromeos/ash/components/phonehub/util/histogram_util.h.
 */
export enum PhoneHubPermissionsSetupAction {
  UNKNOWN = 0,
  SHOWN = 1,
  LEARN_MORE = 2,
  CANCEL = 3,
  DONE = 4,
  NEXT_OR_TRY_AGAIN = 5,
}

/**
 * Numerical values the set up mode in combined set up dialog only.
 * Update of this enum should be propagate to PermissionsOnboardingSetUpMode in
 * chromeos/ash/components/phonehub/util/histogram_util.h.
 */
export enum PhoneHubPermissionsSetupFeatureCombination {
  NONE = 0,
  NOTIFICATION = 1,
  MESSAGING_APP = 2,
  CAMERA_ROLL = 3,
  NOTIFICATION_AND_MESSAGING_APP = 4,
  NOTIFICATION_AND_CAMERA_ROLL = 5,
  MESSAGING_APP_AND_CAMERA_ROLL = 6,
  ALL_PERMISSONS = 7,
}

/**
 * Container for the initial data that the page requires in order to display
 * the correct content. It is also used for receiving status updates during
 * use. Note that the host device may be verified (enabled or disabled),
 * awaiting verification, or it may have failed setup because it was not able
 * to connect to the server.
 *
 * For each MultiDevice feature (including the "suite" feature, which acts as
 * a gatekeeper for the others), the corresponding *State property is an enum
 * containing the data necessary to display it. Note that hostDeviceName
 * should be undefined if and only if no host has been set up, regardless of
 * whether there are potential hosts on the account.
 */
export interface MultiDevicePageContentData {
  mode: MultiDeviceSettingsMode;
  hostDeviceName: string|undefined;
  betterTogetherState: MultiDeviceFeatureState;
  instantTetheringState: MultiDeviceFeatureState;
  smartLockState: MultiDeviceFeatureState;
  phoneHubState: MultiDeviceFeatureState;
  phoneHubCameraRollState: MultiDeviceFeatureState;
  phoneHubNotificationsState: MultiDeviceFeatureState;
  phoneHubTaskContinuationState: MultiDeviceFeatureState;
  phoneHubAppsState: MultiDeviceFeatureState;
  wifiSyncState: MultiDeviceFeatureState;
  cameraRollAccessStatus: PhoneHubFeatureAccessStatus;
  notificationAccessStatus: PhoneHubFeatureAccessStatus;
  appsAccessStatus: PhoneHubFeatureAccessStatus;
  notificationAccessProhibitedReason: PhoneHubFeatureAccessProhibitedReason;
  isNearbyShareDisallowedByPolicy: boolean;
  isPhoneHubPermissionsDialogSupported: boolean;
  isCameraRollFilePermissionGranted: boolean;
  isPhoneHubFeatureCombinedSetupSupported: boolean;
  isChromeOSSyncedSessionSharingEnabled: boolean;
  isLacrosTabSyncEnabled: boolean;
}
