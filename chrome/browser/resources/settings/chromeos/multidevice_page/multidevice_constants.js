// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {

  /**
   * The state of the preference controlling Smart Lock's ability to sign-in the
   * user.
   * @enum {string}
   */
  /* #export */ const SmartLockSignInEnabledState = {
    ENABLED: 'enabled',
    DISABLED: 'disabled',
  };

  /**
   * The possible statuses of hosts on the logged in account that determine the
   * page content. Note that this is based on (and must include an analog of
   * all values in) the HostStatus enum in
   * services/multidevice_setup/public/mojom/multidevice_setup.mojom.
   * @enum {number}
   */
  /* #export */ const MultiDeviceSettingsMode = {
    NO_ELIGIBLE_HOSTS: 0,
    NO_HOST_SET: 1,
    HOST_SET_WAITING_FOR_SERVER: 2,
    HOST_SET_WAITING_FOR_VERIFICATION: 3,
    HOST_SET_VERIFIED: 4,
  };

  /**
   * Enum of MultiDevice features. Note that this is copied from (and must
   * include an analog of all values in) the Feature enum in
   * //chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.
   * @enum {number}
   */
  /* #export */ const MultiDeviceFeature = {
    BETTER_TOGETHER_SUITE: 0,
    INSTANT_TETHERING: 1,
    MESSAGES: 2,
    SMART_LOCK: 3,
    PHONE_HUB: 4,
    PHONE_HUB_NOTIFICATIONS: 5,
    PHONE_HUB_NOTIFICATION_BADGE: 6,
    PHONE_HUB_TASK_CONTINUATION: 7,
    WIFI_SYNC: 8,
  };

  /**
   * Possible states of MultiDevice features. Note that this is copied from (and
   * must include an analog of all values in) the FeatureState enum in
   * //chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.
   * @enum {number}
   */
  /* #export */ const MultiDeviceFeatureState = {
    PROHIBITED_BY_POLICY: 0,
    DISABLED_BY_USER: 1,
    ENABLED_BY_USER: 2,
    NOT_SUPPORTED_BY_CHROMEBOOK: 3,
    NOT_SUPPORTED_BY_PHONE: 4,
    UNAVAILABLE_NO_VERIFIED_HOST: 5,
    UNAVAILABLE_INSUFFICIENT_SECURITY: 6,
    UNAVAILABLE_SUITE_DISABLED: 7,
    FURTHER_SETUP_REQUIRED: 8,
    UNAVAILABLE_TOP_LEVEL_FEATURE_DISABLED: 9,
  };

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
   *
   * @typedef {{
   *   mode: !settings.MultiDeviceSettingsMode,
   *   hostDeviceName: (string|undefined),
   *   betterTogetherState: !settings.MultiDeviceFeatureState,
   *   instantTetheringState: !settings.MultiDeviceFeatureState,
   *   messagesState: !settings.MultiDeviceFeatureState,
   *   smartLockState: !settings.MultiDeviceFeatureState,
   *   phoneHubState: !settings.MultiDeviceFeatureState,
   *   phoneHubNotificationsState: !settings.MultiDeviceFeatureState,
   *   phoneHubNotificationBadgeState: !settings.MultiDeviceFeatureState,
   *   phoneHubTaskContinuationState: !settings.MultiDeviceFeatureState,
   *   wifiSyncState: !settings.MultiDeviceFeatureState,
   *   isAndroidSmsPairingComplete: boolean,
   *   isNotificationAccessGranted: boolean
   * }}
   */
  /* #export */ let MultiDevicePageContentData;

  // #cr_define_end
  return {
    MultiDeviceSettingsMode,
    MultiDeviceFeature,
    MultiDeviceFeatureState,
    MultiDevicePageContentData,
    SmartLockSignInEnabledState
  };
});
