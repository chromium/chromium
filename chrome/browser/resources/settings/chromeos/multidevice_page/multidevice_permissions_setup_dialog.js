// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './multidevice_screen_lock_subpage.js';
import '../os_icons.js';
import '../../settings_shared_css.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from './multidevice_browser_proxy.js';
import {MultiDeviceFeature, PhoneHubPermissionsSetupAction, PhoneHubPermissionsSetupFlowScreens} from './multidevice_constants.js';

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
export const PermissionsSetupStatus = {
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
export const SetupFlowStatus = {
  INTRO: 0,
  SET_LOCKSCREEN: 1,
  WAIT_FOR_PHONE_NOTIFICATION: 2,
  WAIT_FOR_PHONE_APPS: 3,
  WAIT_FOR_PHONE_COMBINED: 4,
};

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-multidevice-permissions-setup-dialog',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private {!PhoneHubPermissionsSetupFlowScreens} */
    setupScreen_: {
      type: Number,
      computed: 'getCurrentScreen_(setupState_, flowState_)',
    },

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

    /** Whether this dialog should show Camera Roll info */
    showCameraRoll: {
      type: Boolean,
      value: false,
    },

    /** Whether this dialog should show Notifications info */
    showNotifications: {
      type: Boolean,
      value: false,
    },

    /** Whether this dialog should show App Streaming info */
    showAppStreaming: {
      type: Boolean,
      value: false,
    },

    /** @private */
    shouldShowLearnMoreButton_: {
      type: Boolean,
      computed: 'computeShouldShowLearnMoreButton_(setupState_, flowState_)',
      reflectToAttribute: true,
    },

    /** @private */
    shouldShowDisabledDoneButton_: {
      type: Boolean,
      computed: 'computeShouldShowDisabledDoneButton_(setupState_)',
      reflectToAttribute: true,
    },

    /**
     * Whether the combined setup for Notifications and Camera Roll is supported
     * on the connected phone.
     */
    combinedSetupSupported: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {?MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = MultiDeviceBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'settings.onNotificationAccessSetupStatusChanged',
        this.onNotificationSetupStateChanged_.bind(this));
    this.addWebUIListener(
        'settings.onAppsAccessSetupStatusChanged',
        this.onAppsSetupStateChanged_.bind(this));
    this.addWebUIListener(
        'settings.onCombinedAccessSetupStatusChanged',
        this.onCombinedSetupStateChanged_.bind(this));
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
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS, true);

    if (this.showAppStreaming) {
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
      this.browserProxy_.setFeatureEnabledState(MultiDeviceFeature.ECHE, true);
    }
  },

  /**
   * @param {!PermissionsSetupStatus} setupState
   * @private
   */
  onCombinedSetupStateChanged_(setupState) {
    if (this.flowState_ !== SetupFlowStatus.WAIT_FOR_PHONE_COMBINED) {
      return;
    }

    this.setupState_ = setupState;
    if (this.setupState_ !== PermissionsSetupStatus.COMPLETED_SUCCESSFULLY) {
      return;
    }

    this.browserProxy_.logPhoneHubPermissionSetUpScreenAction(
        this.setupScreen_, PhoneHubPermissionsSetupAction.SHOWN);

    if (this.showCameraRoll) {
      this.browserProxy_.setFeatureEnabledState(
          MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL, true);
    }
    if (this.showNotifications) {
      this.browserProxy_.setFeatureEnabledState(
          MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS, true);
    }

    if (this.showAppStreaming) {
      this.browserProxy_.attemptAppsSetup();
      this.flowState_ = SetupFlowStatus.WAIT_FOR_PHONE_APPS;
      this.setupState_ = PermissionsSetupStatus.CONNECTION_REQUESTED;
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

  /** @private */
  nextPage_() {
    this.browserProxy_.logPhoneHubPermissionSetUpScreenAction(
        this.setupScreen_, PhoneHubPermissionsSetupAction.NEXT_OR_TRY_AGAIN);
    const isScreenLockRequired =
        this.isScreenLockRequired_();
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

    if ((this.showCameraRoll || this.showNotifications) &&
        this.combinedSetupSupported) {
      this.browserProxy_.attemptCombinedFeatureSetup(
          this.showCameraRoll, this.showNotifications);
      this.flowState_ = SetupFlowStatus.WAIT_FOR_PHONE_COMBINED;
      this.setupState_ = PermissionsSetupStatus.CONNECTION_REQUESTED;
    } else if (this.showNotifications && !this.combinedSetupSupported) {
      this.browserProxy_.attemptNotificationSetup();
      this.flowState_ = SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION;
      this.setupState_ = PermissionsSetupStatus.CONNECTION_REQUESTED;
    } else if (this.showAppStreaming) {
      this.browserProxy_.attemptAppsSetup();
      this.flowState_ = SetupFlowStatus.WAIT_FOR_PHONE_APPS;
      this.setupState_ = PermissionsSetupStatus.CONNECTION_REQUESTED;
    }
  },

  /** @private */
  onCancelClicked_() {
    if (this.flowState_ === SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION) {
      this.browserProxy_.cancelNotificationSetup();
    } else if (this.flowState_ === SetupFlowStatus.WAIT_FOR_PHONE_APPS) {
      this.browserProxy_.cancelAppsSetup();
    } else if (this.flowState_ === SetupFlowStatus.WAIT_FOR_PHONE_COMBINED) {
      this.browserProxy_.cancelCombinedFeatureSetup();
    }
    this.browserProxy_.logPhoneHubPermissionSetUpScreenAction(
        this.setupScreen_, PhoneHubPermissionsSetupAction.CANCEL);
    this.$.dialog.close();
  },

  /** @private */
  onDoneOrCloseButtonClicked_() {
    this.browserProxy_.logPhoneHubPermissionSetUpScreenAction(
        this.setupScreen_, PhoneHubPermissionsSetupAction.DONE);
    this.$.dialog.close();
  },

  /** @private */
  onLearnMoreClicked_() {
    this.browserProxy_.logPhoneHubPermissionSetUpScreenAction(
        this.setupScreen_, PhoneHubPermissionsSetupAction.LEARN_MORE);
    window.open(this.i18n('multidevicePhoneHubPermissionsLearnMoreURL'));
  },

  /** @private */
  getCurrentScreen_() {
    if (this.flowState_ === SetupFlowStatus.INTRO) {
      return PhoneHubPermissionsSetupFlowScreens.INTRO;
    }

    if (this.flowState_ === SetupFlowStatus.SET_LOCKSCREEN) {
      return PhoneHubPermissionsSetupFlowScreens.SET_A_PIN_OR_PASSWORD;
    }

    const Status = PermissionsSetupStatus;
    switch (this.setupState_) {
      case Status.CONNECTION_REQUESTED:
      case Status.CONNECTING:
        return PhoneHubPermissionsSetupFlowScreens.CONNECTING;
      case Status.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE:
        return PhoneHubPermissionsSetupFlowScreens.FINISH_SET_UP_ON_PHONE;
      case Status.COMPLETED_SUCCESSFULLY:
        return PhoneHubPermissionsSetupFlowScreens.CONNECTED;
      case Status.TIMED_OUT_CONNECTING:
        return PhoneHubPermissionsSetupFlowScreens.CONNECTION_TIME_OUT;
      case Status.CONNECTION_DISCONNECTED:
        return PhoneHubPermissionsSetupFlowScreens.CONNECTION_ERROR;
      default:
        return PhoneHubPermissionsSetupFlowScreens.NOT_APPLICABLE;
    }
  },

  /**
   * @return {string} The title of the dialog.
   * @private
   */
  getTitle_() {
    if (this.flowState_ === SetupFlowStatus.INTRO) {
      return this.i18n('multidevicePermissionsSetupAckTitle');
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
        return this.i18n('multidevicePermissionsSetupAwaitingResponseTitle');
      case Status.COMPLETED_SUCCESSFULLY:
        return this.i18n('multidevicePermissionsSetupCompletedTitle');
      case Status.TIMED_OUT_CONNECTING:
        return this.i18n(
            'multidevicePermissionsSetupCouldNotEstablishConnectionTitle');
      case Status.CONNECTION_DISCONNECTED:
        return this.i18n(
            'multideviceNotificationAccessSetupConnectionLostWithPhoneTitle');
      case Status.NOTIFICATION_ACCESS_PROHIBITED:
        return this.i18n(
            'multidevicePermissionsSetupNotificationAccessProhibitedTitle');
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
      return '';
    }

    if (this.flowState_ === SetupFlowStatus.SET_LOCKSCREEN) {
      return '';
    }

    const Status = PermissionsSetupStatus;
    switch (this.setupState_) {
      case Status.COMPLETED_SUCCESSFULLY:
        return this.i18n('multidevicePermissionsSetupCompletedSummary');
      case Status.TIMED_OUT_CONNECTING:
        return this.i18n('multidevicePermissionsSetupEstablishFailureSummary');
      case Status.CONNECTION_DISCONNECTED:
        return this.i18n('multidevicePermissionsSetupMaintainFailureSummary');
      case Status.NOTIFICATION_ACCESS_PROHIBITED:
        return this.i18nAdvanced(
            'multidevicePermissionsSetupNotificationAccessProhibitedSummary');
      case Status.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE:
        return this.i18n('multidevicePermissionsSetupOperationsInstructions');
      case Status.CONNECTION_REQUESTED:
      case Status.CONNECTING:
        return this.i18n('multideviceNotificationAccessSetupInstructions');
      default:
        return '';
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowLearnMoreButton_() {
    return this.flowState_ === SetupFlowStatus.INTRO ||
        this.setupState_ ===
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE;
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
  computeShouldShowDisabledDoneButton_() {
    return this.setupState_ ===
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE;
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

  /**
   * @return {boolean}
   * @private
   */
  isScreenLockRequired_() {
    return loadTimeData.getBoolean('isEcheAppEnabled') &&
        loadTimeData.getBoolean('isPhoneScreenLockEnabled') &&
        !loadTimeData.getBoolean('isChromeosScreenLockEnabled') &&
        this.showAppStreaming;
  },
});
