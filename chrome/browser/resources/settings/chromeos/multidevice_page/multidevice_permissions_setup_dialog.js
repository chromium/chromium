// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This element provides the Phone Hub notification and apps access setup flow
 * that, when successfully completed, enables the feature that allows a user's
 * phone notifications and apps to be mirrored on their Chromebook.
 */

/**
 * Numerical values should not be changed because they must stay in sync with
 * notification_access_setup_operation.h and apps_access_setup_operation.h,
 * with the exception of CONNECTION_REQUESTED.
 * @enum {number}
 */
/* #export */ const PermissionsSetupStatus = {
  CONNECTION_REQUESTED: 0,
  CONNECTING: 1,
  TIMED_OUT_CONNECTING: 2,
  CONNECTION_DISCONNECTED: 3,
  SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE: 4,
  COMPLETED_SUCCESSFULLY: 5,
  NOTIFICATION_ACCESS_PROHIBITED: 6,
};

/**
 * Numerical values the flow of dialog set up progress.
 * @enum {number}
 */
/* #export */ const SetupFlowStatus = {
  INTRO: 0,
  SET_LOCKSCREEN: 1,
  WAIT_FOR_PHONE_NOTIFICATION: 2,
  WAIT_FOR_PHONE_APPS: 3,
};

Polymer({
  is: 'settings-multidevice-permissions-setup-dialog',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * A null |setupState_| indicates that the operation has not yet started.
     * @private {?PermissionsSetupStatus}
     */
    setupState_: {
      type: Number,
      value: null,
    },

    /** @private */
    title_: {
      type: String,
      computed: 'getTitle_(setupState_, flowState_)',
    },

    /** @private */
    description_: {
      type: String,
      computed: 'getDescription_(setupState_, flowState_)',
    },

    /** @private */
    hasStartedSetupAttempt_: {
      type: Boolean,
      computed: 'computeHasStartedSetupAttempt_(flowState_)',
      reflectToAttribute: true,
    },

    /** @private */
    isSetupAttemptInProgress_: {
      type: Boolean,
      computed: 'computeIsSetupAttemptInProgress_(setupState_)',
      reflectToAttribute: true,
    },

    /** @private */
    didSetupAttemptFail_: {
      type: Boolean,
      computed: 'computeDidSetupAttemptFail_(setupState_)',
      reflectToAttribute: true,
    },

    /** @private */
    hasCompletedSetupSuccessfully_: {
      type: Boolean,
      computed: 'computeHasCompletedSetupSuccessfully_(setupState_)',
      reflectToAttribute: true,
    },

    /** @private */
    isNotificationAccessProhibited_: {
      type: Boolean,
      computed: 'computeIsNotificationAccessProhibited_(setupState_)',
    },

    /** @private */
    shouldShowSetupInstructionsSeparately_: {
      type: Boolean,
      computed: 'computeShouldShowSetupInstructionsSeparately_(' +
          'setupState_)',
      reflectToAttribute: true,
    },

    /**
     * @private {?SetupFlowStatus}
     */
    flowState_: {
      type: Number,
      value: SetupFlowStatus.INTRO,
    },

    /** @private */
    isScreenLockEnabled_: {
      type: Boolean,
      value: false,
    },

    /** Reflects whether the password dialog is showing. */
    isPasswordDialogShowing: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /**
     * @private {?settings.PhoneHubPermissionsSetupMode}
     */
    phonePermissionSetupMode: {
      type: Number,
      value: settings.PhoneHubPermissionsSetupMode.INIT_MODE,
      notify: true,
    },
  },

  /** @private {?settings.MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = settings.MultiDeviceBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'settings.onNotificationAccessSetupStatusChanged',
        this.onNotificationSetupStateChanged_.bind(this));
    this.addWebUIListener(
        'settings.onAppsAccessSetupStatusChanged',
        this.onAppsSetupStateChanged_.bind(this));
    this.$.dialog.showModal();
  },

  /**
   * @param {!PermissionsSetupStatus} setupState
   * @private
   */
  onNotificationSetupStateChanged_(setupState) {
    if (this.flowState_ !== SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION) {
      return;
    }

    this.setupState_ = setupState;
    if (this.setupState_ !== PermissionsSetupStatus.COMPLETED_SUCCESSFULLY) {
      return;
    }

    this.browserProxy_.setFeatureEnabledState(
        settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS, true);

    if (this.phonePermissionSetupMode ===
        PhoneHubPermissionsSetupMode.ALL_PERMISSIONS_SETUP_MODE) {
      this.browserProxy_.attemptAppsSetup();
      this.flowState_ = SetupFlowStatus.WAIT_FOR_PHONE_APPS;
      this.setupState_ = PermissionsSetupStatus.CONNECTION_REQUESTED;
    }
  },

  /**
   * @param {!PermissionsSetupStatus} setupState
   * @private
   */
  onAppsSetupStateChanged_(setupState) {
    if (this.flowState_ !== SetupFlowStatus.WAIT_FOR_PHONE_APPS) {
      return;
    }

    this.setupState_ = setupState;

    if (this.setupState_ === PermissionsSetupStatus.COMPLETED_SUCCESSFULLY) {
      this.browserProxy_.setFeatureEnabledState(
          settings.MultiDeviceFeature.ECHE, true);
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasStartedSetupAttempt_() {
    return this.flowState_ !== SetupFlowStatus.INTRO;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsSetupAttemptInProgress_() {
    return this.setupState_ ===
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE ||
        this.setupState_ === PermissionsSetupStatus.CONNECTING ||
        this.setupState_ === PermissionsSetupStatus.CONNECTION_REQUESTED;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasCompletedSetupSuccessfully_() {
    return this.setupState_ === PermissionsSetupStatus.COMPLETED_SUCCESSFULLY;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsNotificationAccessProhibited_() {
    return this.setupState_ ===
        PermissionsSetupStatus.NOTIFICATION_ACCESS_PROHIBITED;
  },

  /**
   * @return {boolean}
   * @private
   * */
  computeDidSetupAttemptFail_() {
    return this.setupState_ === PermissionsSetupStatus.TIMED_OUT_CONNECTING ||
        this.setupState_ === PermissionsSetupStatus.CONNECTION_DISCONNECTED ||
        this.setupState_ ===
        PermissionsSetupStatus.NOTIFICATION_ACCESS_PROHIBITED;
  },

  /**
   * @return {boolean} Whether to show setup instructions in its own section.
   * @private
   */
  computeShouldShowSetupInstructionsSeparately_() {
    return this.setupState_ === null ||
        this.setupState_ === PermissionsSetupStatus.CONNECTION_REQUESTED ||
        this.setupState_ ===
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE ||
        this.setupState_ === PermissionsSetupStatus.CONNECTING;
  },

  /** @private */
  nextPage_() {
    const isScreenLockRequired = loadTimeData.getBoolean('isEcheAppEnabled') &&
        loadTimeData.getBoolean('isPhoneScreenLockEnabled') &&
        !loadTimeData.getBoolean('isChromeosScreenLockEnabled');
    switch (this.flowState_) {
      case SetupFlowStatus.INTRO:
        if (isScreenLockRequired) {
          this.flowState_ = SetupFlowStatus.SET_LOCKSCREEN;
          return;
        }
        break;
      case SetupFlowStatus.SET_LOCKSCREEN:
        if (!this.isScreenLockEnabled_) {
          return;
        }
        this.isPasswordDialogShowing = false;
        break;
    }

    switch (this.phonePermissionSetupMode) {
      case settings.PhoneHubPermissionsSetupMode.APPS_SETUP_MODE:
        this.browserProxy_.attemptAppsSetup();
        this.flowState_ = SetupFlowStatus.WAIT_FOR_PHONE_APPS;
        this.setupState_ = PermissionsSetupStatus.CONNECTION_REQUESTED;
        break;
      case settings.PhoneHubPermissionsSetupMode.NOTIFICATION_SETUP_MODE:
      case settings.PhoneHubPermissionsSetupMode.ALL_PERMISSIONS_SETUP_MODE:
        this.browserProxy_.attemptNotificationSetup();
        this.flowState_ = SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION;
        this.setupState_ = PermissionsSetupStatus.CONNECTION_REQUESTED;
        break;
    }
  },

  /** @private */
  onCancelClicked_() {
    if (this.flowState_ === SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION) {
      this.browserProxy_.cancelNotificationSetup();
    } else if (this.flowState_ === SetupFlowStatus.WAIT_FOR_PHONE_APPS) {
      this.browserProxy_.cancelAppsSetup();
    }
    this.$.dialog.close();
  },

  /** @private */
  onDoneOrCloseButtonClicked_() {
    this.$.dialog.close();
  },

  /** @private */
  onLearnMoreClicked_() {
    window.open(this.i18n('multidevicePhoneHubPermissionsLearnMoreURL'));
  },

  /**
   * @return {string} The title of the dialog.
   * @private
   */
  getTitle_() {
    if (this.flowState_ === SetupFlowStatus.INTRO) {
      return this.i18n('multideviceNotificationAccessSetupAckTitle');
    }
    if (this.flowState_ === SetupFlowStatus.SET_LOCKSCREEN) {
      return this.i18n('multideviceNotificationAccessSetupScreenLockTitle');
    }

    const Status = PermissionsSetupStatus;
    switch (this.setupState_) {
      case Status.CONNECTION_REQUESTED:
      case Status.CONNECTING:
        return this.i18n('multideviceNotificationAccessSetupConnectingTitle');
      case Status.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE:
        return this.i18n(
            'multideviceNotificationAccessSetupAwaitingResponseTitle');
      case Status.COMPLETED_SUCCESSFULLY:
        return this.i18n('multideviceNotificationAccessSetupCompletedTitle');
      case Status.TIMED_OUT_CONNECTING:
        return this.i18n(
            'multideviceNotificationAccessSetupCouldNotEstablishConnectionTitle');
      case Status.CONNECTION_DISCONNECTED:
        return this.i18n(
            'multideviceNotificationAccessSetupConnectionLostWithPhoneTitle');
      case Status.NOTIFICATION_ACCESS_PROHIBITED:
        return this.i18n(
            'multideviceNotificationAccessSetupAccessProhibitedTitle');
      default:
        return '';
    }
  },

  /**
   * @return {string} A description about the connection attempt state.
   * @private
   */
  getDescription_() {
    if (this.flowState_ === SetupFlowStatus.INTRO) {
      return this.i18n('multideviceNotificationAccessSetupAckSummary');
    }

    if (this.flowState_ === SetupFlowStatus.SET_LOCKSCREEN) {
      return '';
    }

    const Status = PermissionsSetupStatus;
    switch (this.setupState_) {
      case Status.COMPLETED_SUCCESSFULLY:
        return this.i18n('multideviceNotificationAccessSetupCompletedSummary');
      case Status.TIMED_OUT_CONNECTING:
        return this.i18n(
            'multideviceNotificationAccessSetupEstablishFailureSummary');
      case Status.CONNECTION_DISCONNECTED:
        return this.i18n(
            'multideviceNotificationAccessSetupMaintainFailureSummary');
      case Status.NOTIFICATION_ACCESS_PROHIBITED:
        return this.i18nAdvanced(
            'multideviceNotificationAccessSetupAccessProhibitedSummary');
      case Status.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE:
        return this.i18n(
            'multideviceNotificationAccessSetupAwaitingResponseSummary');

      // Only setup instructions will be shown.
      case Status.CONNECTION_REQUESTED:
      case Status.CONNECTING:
      default:
        return '';
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowCancelButton_() {
    return this.setupState_ !== PermissionsSetupStatus.COMPLETED_SUCCESSFULLY &&
        this.setupState_ !==
        PermissionsSetupStatus.NOTIFICATION_ACCESS_PROHIBITED;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowTryAgainButton_() {
    return this.setupState_ === PermissionsSetupStatus.TIMED_OUT_CONNECTING ||
        this.setupState_ === PermissionsSetupStatus.CONNECTION_DISCONNECTED;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowScreenLockInstructions_() {
    return this.flowState_ === SetupFlowStatus.SET_LOCKSCREEN;
  },
});
