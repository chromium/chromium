// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This element provides the Phone Hub notification and apps access setup flow
 * that, when successfully completed, enables the feature that allows a user's
 * phone notifications and apps to be mirrored on their Chromebook.
 */

import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../os_settings_icons.html.js';
import '../settings_shared.css.js';
import './multidevice_screen_lock_subpage.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LockStateMixin} from '../lock_state_mixin.js';

import {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from './multidevice_browser_proxy.js';
import {MultiDeviceFeature, PhoneHubPermissionsSetupAction, PhoneHubPermissionsSetupFeatureCombination, PhoneHubPermissionsSetupFlowScreens} from './multidevice_constants.js';
import {getTemplate} from './multidevice_permissions_setup_dialog.html.js';
import {SettingsMultideviceScreenLockSubpageElement} from './multidevice_screen_lock_subpage.js';

/**
 * Numerical values should not be changed because they must stay in sync with
 * notification_access_setup_operation.h and apps_access_setup_operation.h,
 * with the exception of CONNECTION_REQUESTED. If PermissionsSetupStatus is
 * FAILED_OR_CANCELLED, we will abort all setup processes. If
 * PermissionsSetupStatus is COMPLETED_USER_REJECTED, we will proceed to the
 * next setup process.
 */
export enum PermissionsSetupStatus {
  CONNECTION_REQUESTED = 0,
  CONNECTING = 1,
  TIMED_OUT_CONNECTING = 2,
  CONNECTION_DISCONNECTED = 3,
  SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE = 4,
  COMPLETED_SUCCESSFULLY = 5,
  NOTIFICATION_ACCESS_PROHIBITED = 6,
  COMPLETED_USER_REJECTED = 7,
  FAILED_OR_CANCELLED = 8,
  CAMERA_ROLL_GRANTED_NOTIFICATION_REJECTED = 9,
  CAMERA_ROLL_REJECTED_NOTIFICATION_GRANTED = 10,
  CONNECTION_ESTABLISHED = 11,
}

/**
 * Numerical values the flow of dialog set up progress.
 */
export enum SetupFlowStatus {
  INTRO = 0,
  SET_LOCKSCREEN = 1,
  WAIT_FOR_PHONE_NOTIFICATION = 2,
  WAIT_FOR_PHONE_APPS = 3,
  WAIT_FOR_PHONE_COMBINED = 4,
  WAIT_FOR_CONNECTION = 5,
  FINISHED = 6,
}

/**
 * Indicates that the onboarding flow includes Phone Hub Notification feature.
 */
export const NOTIFICATION_FEATURE = 1 << 0;

/**
 * Indicates that the onboarding flow includes Phone Hub Camera Roll feature.
 */
export const CAMERA_ROLL_FEATURE = 1 << 1;

/**
 * Indicates that the onboarding flow includes Phone Hub Apps feature.
 */
export const APPS_FEATURE = 1 << 2;

export interface SettingsMultidevicePermissionsSetupDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const SettingsMultidevicePermissionsSetupDialogElementBase =
    LockStateMixin(PolymerElement);

export class SettingsMultidevicePermissionsSetupDialogElement extends
    SettingsMultidevicePermissionsSetupDialogElementBase {
  static get is() {
    return 'settings-multidevice-permissions-setup-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      setupScreen_: {
        type: Number,
        computed: 'getCurrentScreen_(setupState_, flowState_)',
      },

      /**
       * A null |setupState_| indicates that the operation has not yet started.
       */
      setupState_: {
        type: Number,
        value: null,
      },

      title_: {
        type: String,
        computed: 'getTitle_(setupState_, flowState_)',
      },

      description_: {
        type: String,
        computed: 'getDescription_(setupState_, flowState_)',
      },

      hasStartedSetupAttempt_: {
        type: Boolean,
        computed: 'computeHasStartedSetupAttempt_(flowState_)',
        reflectToAttribute: true,
      },

      isSetupAttemptInProgress_: {
        type: Boolean,
        computed: 'computeIsSetupAttemptInProgress_(setupState_)',
        reflectToAttribute: true,
      },

      isSetupScreenLockInProgress_: {
        type: Boolean,
        computed: 'computeIsSetupScreenLockInProgress_(flowState_)',
        reflectToAttribute: true,
      },

      didSetupAttemptFail_: {
        type: Boolean,
        computed: 'computeDidSetupAttemptFail_(setupState_)',
        reflectToAttribute: true,
      },

      hasCompletedSetup_: {
        type: Boolean,
        computed: 'computeHasCompletedSetup_(setupState_)',
        reflectToAttribute: true,
      },

      isNotificationAccessProhibited_: {
        type: Boolean,
        computed: 'computeIsNotificationAccessProhibited_(setupState_)',
      },

      flowState_: {
        type: Number,
        value: SetupFlowStatus.INTRO,
      },

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
       * Get the value of settings.OnEnableScreenLockChanged from
       * multidevice_page.js because multidevice_permissions_setup_dialog.js
       * doesn't always popup to receive event from FireWebUIListener.
       */
      isChromeosScreenLockEnabled: {
        type: Boolean,
        value: false,
      },

      /**
       * Get the value of settings.OnScreenLockStatusChanged from
       * multidevice_page.js because multidevice_permissions_setup_dialog.js
       * doesn't always popup to receive event from FireWebUIListener.
       */
      isPhoneScreenLockEnabled: {
        type: Boolean,
        value: false,
      },

      /** Whether this dialog should show Camera Roll info */
      showCameraRoll: {
        type: Boolean,
        value: false,
        observer: 'onAccessStateChanged_',
      },

      /** Whether this dialog should show Notifications info */
      showNotifications: {
        type: Boolean,
        value: false,
        observer: 'onAccessStateChanged_',
      },

      /** Whether this dialog should show App Streaming info */
      showAppStreaming: {
        type: Boolean,
        value: false,
        observer: 'onAccessStateChanged_',
      },

      /**
       * Indicates that the features we want to handle during setup flow.
       * It is constructed using the bitwise _FEATURE values (ex:
       * NOTIFICATION_FEATURE) declared at the top.
       */
      setupMode_: {
        type: Number,
        value: 0,
      },

      /**
       * Indicates that the features we have completed after setup flow.
       * It is constructed using the bitwise _FEATURE values (ex:
       * NOTIFICATION_FEATURE) declared at the top.
       */
      completedMode_: {
        type: Number,
        value: 0,
      },

      shouldShowLearnMoreButton_: {
        type: Boolean,
        computed: 'computeShouldShowLearnMoreButton_(setupState_, flowState_)',
        reflectToAttribute: true,
      },

      shouldShowDisabledDoneButton_: {
        type: Boolean,
        computed: 'computeShouldShowDisabledDoneButton_(setupState_)',
        reflectToAttribute: true,
      },

      isPinNumberSelected_: {
        type: Boolean,
        value: false,
      },

      isPinSet_: {
        type: Boolean,
        value: false,
      },

      showSetupPinDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the combined setup for Notifications and Camera Roll is
       * supported on the connected phone.
       */
      combinedSetupSupported: {
        type: Boolean,
        value: false,
      },

      learnMoreButtonAriaLabel_: {
        type: String,
        computed: 'getLearnMoreButtonAriaLabel_()',
      },
    };
  }

  combinedSetupSupported: boolean;
  isChromeosScreenLockEnabled: boolean;
  isPasswordDialogShowing: boolean;
  isPhoneScreenLockEnabled: boolean;
  showAppStreaming: boolean;
  showCameraRoll: boolean;
  showNotifications: boolean;
  private browserProxy_: MultiDeviceBrowserProxy;
  private completedMode_: number;
  private description_: string;
  private didSetupAttemptFail_: boolean;
  private flowState_: SetupFlowStatus;
  private hasCompletedSetup_: boolean;
  private hasStartedSetupAttempt_: boolean;
  private isNotificationAccessProhibited_: boolean;
  private isPinNumberSelected_: boolean;
  private isPinSet_: boolean;
  private isScreenLockEnabled_: boolean;
  private isSetupAttemptInProgress_: boolean;
  private isSetupScreenLockInProgress_: boolean;
  private learnMoreButtonAriaLabel_: string;
  private setupMode_: number;
  private setupScreen_: PhoneHubPermissionsSetupFlowScreens;
  private setupState_: PermissionsSetupStatus|null;
  private shouldShowDisabledDoneButton_: boolean;
  private shouldShowLearnMoreButton_: boolean;
  private showSetupPinDialog_: boolean;
  private title_: string;

  constructor() {
    super();

    this.browserProxy_ = MultiDeviceBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.addEventListener('set-pin-done', this.onSetPinDone_);
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.addWebUiListener(
        'settings.onNotificationAccessSetupStatusChanged',
        this.onNotificationSetupStateChanged_.bind(this));
    this.addWebUiListener(
        'settings.onAppsAccessSetupStatusChanged',
        this.onAppsSetupStateChanged_.bind(this));
    this.addWebUiListener(
        'settings.onCombinedAccessSetupStatusChanged',
        this.onCombinedSetupStateChanged_.bind(this));
    this.addWebUiListener(
        'settings.onFeatureSetupConnectionStatusChanged',
        this.onFeatureSetupConnectionStatusChanged_.bind(this));
    this.$.dialog.showModal();
    this.browserProxy_.logPhoneHubPermissionSetUpScreenAction(
        PhoneHubPermissionsSetupFlowScreens.INTRO,
        PhoneHubPermissionsSetupAction.SHOWN);
  }

  private onNotificationSetupStateChanged_(notificationSetupState:
                                               PermissionsSetupStatus): void {
    if (this.flowState_ !== SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION) {
      return;
    }

    // When the notificationSetupState is COMPLETED_SUCCESSFULLY or
    // COMPLETED_USER_REJECTED we should continue on with the setup flow if
    // there are additional features, all other results will change the screen
    // that is shown and pause or terminate the setup flow.
    switch (notificationSetupState) {
      case PermissionsSetupStatus.FAILED_OR_CANCELLED:
      case PermissionsSetupStatus.TIMED_OUT_CONNECTING:
      case PermissionsSetupStatus.CONNECTION_DISCONNECTED:
      case PermissionsSetupStatus.NOTIFICATION_ACCESS_PROHIBITED:
        this.flowState_ = SetupFlowStatus.FINISHED;
        break;
      case PermissionsSetupStatus.CONNECTION_REQUESTED:
      case PermissionsSetupStatus.CONNECTING:
      case PermissionsSetupStatus
          .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE:
        this.setupState_ = notificationSetupState;
        return;
      default:
        break;
    }

    // Note: we can only update this.setupState_ after assigning
    // this.completeMode_. Otherwise, we cannot use the final
    // this.completedMode_ to determine the completed title.
    if (notificationSetupState ===
        PermissionsSetupStatus.COMPLETED_SUCCESSFULLY) {
      if (this.setupMode_ & NOTIFICATION_FEATURE && !this.showNotifications) {
        this.completedMode_ |= NOTIFICATION_FEATURE;
        this.browserProxy_.setFeatureEnabledState(
            MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS, true);
      }
    }

    if (this.showAppStreaming) {
      // We still need to process the apps steaming onboarding flow, update
      // this.setupState_ to CONNECTION_REQUESTED first and wait for
      // onAppsSetupStateChanged_() callback to update this.setupState_.
      this.browserProxy_.attemptAppsSetup();
      this.flowState_ = SetupFlowStatus.WAIT_FOR_PHONE_APPS;
      this.setupState_ = PermissionsSetupStatus.CONNECTION_REQUESTED;
    } else {
      this.setupState_ = notificationSetupState;
      this.flowState_ = SetupFlowStatus.FINISHED;
      // We don't need to deal with the apps streaming onboarding flow, so we
      // can log completed case here.
      this.logCompletedSetupModeMetrics_();
    }
  }

  private onAppsSetupStateChanged_(appsSetupResult: PermissionsSetupStatus):
      void {
    if (this.flowState_ !== SetupFlowStatus.WAIT_FOR_PHONE_APPS) {
      return;
    }

    // Note: If appsSetupResult is COMPLETED_SUCCESSFULLY, we can only update
    // this.setupState_ after assigning this.completeMode_. Otherwise, we cannot
    // use the final this.completedMode_ to determine the completed title.
    if (appsSetupResult === PermissionsSetupStatus.COMPLETED_SUCCESSFULLY &&
        !this.showAppStreaming) {
      this.completedMode_ |= APPS_FEATURE;
      this.browserProxy_.setFeatureEnabledState(MultiDeviceFeature.ECHE, true);
    }

    this.setupState_ = appsSetupResult;

    if (appsSetupResult !==
            PermissionsSetupStatus
                .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE &&
        appsSetupResult !== PermissionsSetupStatus.CONNECTING &&
        appsSetupResult !== PermissionsSetupStatus.CONNECTION_REQUESTED) {
      this.flowState_ = SetupFlowStatus.FINISHED;
    }

    if (this.computeHasCompletedSetup_()) {
      this.logCompletedSetupModeMetrics_();
    }
  }

  private onCombinedSetupStateChanged_(combinedSetupResult:
                                           PermissionsSetupStatus): void {
    if (this.flowState_ !== SetupFlowStatus.WAIT_FOR_PHONE_COMBINED) {
      return;
    }

    // When the combinedSetupResult is COMPLETED_SUCCESSFULLY or
    // COMPLETED_USER_REJECTED we should continue on with the setup flow if
    // there are additional features, all other results will change the screen
    // that is shown and pause or terminate the setup flow.
    switch (combinedSetupResult) {
      case PermissionsSetupStatus.COMPLETED_SUCCESSFULLY:
      case PermissionsSetupStatus.COMPLETED_USER_REJECTED:
      case PermissionsSetupStatus.CAMERA_ROLL_GRANTED_NOTIFICATION_REJECTED:
      case PermissionsSetupStatus.CAMERA_ROLL_REJECTED_NOTIFICATION_GRANTED:
        break;
      // TODO(b/266455078) Avoid Fallthrough case in switch
      // @ts-expect-error Fallthrough case in switch
      case PermissionsSetupStatus.FAILED_OR_CANCELLED:
        this.updateCamearRollSetupResultIfNeeded_();
      case PermissionsSetupStatus.TIMED_OUT_CONNECTING:
      case PermissionsSetupStatus.CONNECTION_DISCONNECTED:
      // TODO(b/266455078) Avoid Fallthrough case in switch
      // @ts-expect-error Fallthrough case in switch
      case PermissionsSetupStatus.NOTIFICATION_ACCESS_PROHIBITED:
        this.flowState_ = SetupFlowStatus.FINISHED;
      case PermissionsSetupStatus.CONNECTION_REQUESTED:
      case PermissionsSetupStatus.CONNECTING:
      case PermissionsSetupStatus
          .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE:
        this.setupState_ = combinedSetupResult;
        return;
    }

    // Note: we can only update this.setupState_ after assigning
    // this.completeMode_. Otherwise, we cannot use the final
    // this.completedMode_ to determine the completed title.
    if (combinedSetupResult === PermissionsSetupStatus.COMPLETED_SUCCESSFULLY) {
      this.updateCamearRollSetupResultIfNeeded_();
      this.updateNotificationsSetupResultIfNeeded_();
    }

    if (combinedSetupResult ===
        PermissionsSetupStatus.CAMERA_ROLL_GRANTED_NOTIFICATION_REJECTED) {
      this.updateCamearRollSetupResultIfNeeded_();
    }

    if (combinedSetupResult ===
        PermissionsSetupStatus.CAMERA_ROLL_REJECTED_NOTIFICATION_GRANTED) {
      this.updateNotificationsSetupResultIfNeeded_();
    }

    if (this.showAppStreaming) {
      // We still need to process the apps steaming onboarding flow, update
      // this.setupState_ to CONNECTION_REQUESTED first and wait for
      // onAppsSetupStateChanged_() callback to update this.setupState_.
      this.browserProxy_.attemptAppsSetup();
      this.flowState_ = SetupFlowStatus.WAIT_FOR_PHONE_APPS;
      this.setupState_ = PermissionsSetupStatus.CONNECTION_REQUESTED;
    } else {
      this.setupState_ = combinedSetupResult;
      this.flowState_ = SetupFlowStatus.FINISHED;
      // We don't need to deal with the apps streaming onboarding flow, so we
      // can log completed case here.
      this.logCompletedSetupModeMetrics_();
    }
  }

  private logSetupModeMetrics_(): void {
    if (this.showCameraRoll) {
      this.setupMode_ |= CAMERA_ROLL_FEATURE;
    }
    if (this.showNotifications) {
      this.setupMode_ |= NOTIFICATION_FEATURE;
    }
    if (this.showAppStreaming) {
      this.setupMode_ |= APPS_FEATURE;
    }
    this.browserProxy_.logPhoneHubPermissionOnboardingSetupMode(
        this.computePhoneHubPermissionsSetupMode_(this.setupMode_));
  }

  private logCompletedSetupModeMetrics_(): void {
    this.browserProxy_.logPhoneHubPermissionSetUpScreenAction(
        this.setupScreen_, PhoneHubPermissionsSetupAction.SHOWN);
    this.browserProxy_.logPhoneHubPermissionOnboardingSetupResult(
        this.computePhoneHubPermissionsSetupMode_(this.completedMode_));
  }

  private onFeatureSetupConnectionStatusChanged_(
      connectionResult: PermissionsSetupStatus): void {
    if (this.flowState_ !== SetupFlowStatus.WAIT_FOR_CONNECTION) {
      return;
    }

    switch (connectionResult) {
      case PermissionsSetupStatus.TIMED_OUT_CONNECTING:
      // TODO(b/266455078) Avoid Fallthrough case in switch
      // @ts-expect-error Fallthrough case in switch
      case PermissionsSetupStatus.CONNECTION_DISCONNECTED:
        this.setupState_ = connectionResult;
      case PermissionsSetupStatus.COMPLETED_SUCCESSFULLY:
        return;
      case PermissionsSetupStatus.CONNECTION_ESTABLISHED:
        // Make sure FeatureSetupConnectionOperation ends properly.
        this.browserProxy_.cancelFeatureSetupConnection();
        if (this.isScreenLockRequired_()) {
          this.flowState_ = SetupFlowStatus.SET_LOCKSCREEN;
          return;
        }
        this.startSetupProcess_();
        return;
      default:
        return;
    }
  }

  private updateCamearRollSetupResultIfNeeded_(): void {
    if (this.setupMode_ & CAMERA_ROLL_FEATURE && !this.showCameraRoll) {
      this.completedMode_ |= CAMERA_ROLL_FEATURE;
      this.browserProxy_.setFeatureEnabledState(
          MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL, true);
    }
  }

  private updateNotificationsSetupResultIfNeeded_(): void {
    if (this.setupMode_ & NOTIFICATION_FEATURE && !this.showNotifications) {
      this.completedMode_ |= NOTIFICATION_FEATURE;
      this.browserProxy_.setFeatureEnabledState(
          MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS, true);
    }
  }

  private computeHasStartedSetupAttempt_(): boolean {
    return this.flowState_ !== SetupFlowStatus.INTRO;
  }

  private computeIsSetupAttemptInProgress_(): boolean {
    return this.setupState_ ===
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE ||
        this.setupState_ === PermissionsSetupStatus.CONNECTING ||
        this.setupState_ === PermissionsSetupStatus.CONNECTION_REQUESTED;
  }

  private computeIsSetupScreenLockInProgress_(): boolean {
    return this.flowState_ === SetupFlowStatus.SET_LOCKSCREEN;
  }

  private computeHasCompletedSetup_(): boolean {
    return this.setupState_ === PermissionsSetupStatus.COMPLETED_SUCCESSFULLY ||
        this.someFeaturesHaveBeenSetupWhenCompleted_() ||
        this.setupState_ ===
        PermissionsSetupStatus.CAMERA_ROLL_GRANTED_NOTIFICATION_REJECTED ||
        this.setupState_ ===
        PermissionsSetupStatus.CAMERA_ROLL_REJECTED_NOTIFICATION_GRANTED;
  }

  private computeIsNotificationAccessProhibited_(): boolean {
    return this.setupState_ ===
        PermissionsSetupStatus.NOTIFICATION_ACCESS_PROHIBITED;
  }

  private computeDidSetupAttemptFail_(): boolean {
    return this.setupState_ === PermissionsSetupStatus.TIMED_OUT_CONNECTING ||
        this.setupState_ === PermissionsSetupStatus.CONNECTION_DISCONNECTED ||
        this.setupState_ ===
        PermissionsSetupStatus.NOTIFICATION_ACCESS_PROHIBITED ||
        this.noFeatureHasBeenSetupWhenCompleted_();
  }

  private someFeaturesHaveBeenSetupWhenCompleted_(): boolean {
    return (this.setupState_ ===
                PermissionsSetupStatus.COMPLETED_USER_REJECTED ||
            this.setupState_ === PermissionsSetupStatus.FAILED_OR_CANCELLED) &&
        this.completedMode_ !== 0;
  }

  private noFeatureHasBeenSetupWhenCompleted_(): boolean {
    return (this.setupState_ ===
                PermissionsSetupStatus.COMPLETED_USER_REJECTED ||
            this.setupState_ === PermissionsSetupStatus.FAILED_OR_CANCELLED) &&
        this.completedMode_ === 0;
  }

  // Retrieves whether the user has a fully configured PIN. Must only be called
  // if the screen-lock-subpage element is currently attached to the DOM.
  private hasPin_(): boolean {
    // We retrieve the screen-lock-subpage child element directly with
    // |getElementById| because |this.$| is populated only once during
    // initialization of |this| element. Thus, if |screen-lock-subpage| is
    // attached only later (e.g. because of a |dom-if|), then it won't appear
    // in |this.$|.
    assert(this.shadowRoot !== null);
    const screenLockSubpage =
        this.shadowRoot.getElementById('screen-lock-subpage');
    assert(
        screenLockSubpage instanceof
        SettingsMultideviceScreenLockSubpageElement);
    return screenLockSubpage.hasPin;
  }

  private nextPage_(): void {
    this.browserProxy_.logPhoneHubPermissionSetUpScreenAction(
        this.getCurrentScreen_(),
        PhoneHubPermissionsSetupAction.NEXT_OR_TRY_AGAIN);

    // Undefined behavior can happen when the current page has focus on the
    // "next" button, however the next page hides the button. This prevents
    // undefined behavior by focusing on the dialog before changing screens.
    this.$.dialog.focus();

    switch (this.flowState_) {
      // TODO(b/266455078) Avoid Fallthrough case in switch
      // @ts-expect-error Fallthrough case in switch
      case SetupFlowStatus.INTRO:
        this.logSetupModeMetrics_();
      // TODO(b/266455078) Avoid Fallthrough case in switch
      // @ts-expect-error Fallthrough case in switch
      case SetupFlowStatus.FINISHED:
        this.flowState_ = SetupFlowStatus.WAIT_FOR_CONNECTION;
      case SetupFlowStatus.WAIT_FOR_CONNECTION:
        this.browserProxy_.attemptFeatureSetupConnection();
        this.setupState_ = PermissionsSetupStatus.CONNECTION_REQUESTED;
        return;
      case SetupFlowStatus.SET_LOCKSCREEN:
        if (!this.isScreenLockEnabled_) {
          return;
        }
        if (this.isPinNumberSelected_ && !this.isPinSet_ && !this.hasPin_()) {
          // When users select pin number and click next button, popup set pin
          // dialog.
          this.showSetupPinDialog_ = true;
          this.propagatePinNumberSelected_(true);
          return;
        }
        this.propagatePinNumberSelected_(false);
        this.isPasswordDialogShowing = false;
        break;
    }

    this.startSetupProcess_();
  }

  private startSetupProcess_(): void {
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
  }

  private onCancelClicked_(): void {
    if (this.flowState_ === SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION) {
      this.browserProxy_.cancelNotificationSetup();
    } else if (this.flowState_ === SetupFlowStatus.WAIT_FOR_PHONE_APPS) {
      this.browserProxy_.cancelAppsSetup();
    } else if (this.flowState_ === SetupFlowStatus.WAIT_FOR_PHONE_COMBINED) {
      this.browserProxy_.cancelCombinedFeatureSetup();
    } else if (this.flowState_ === SetupFlowStatus.WAIT_FOR_CONNECTION) {
      this.browserProxy_.cancelFeatureSetupConnection();
    }
    if (this.noFeatureHasBeenSetupWhenCompleted_()) {
      this.logCompletedSetupModeMetrics_();
    }
    this.browserProxy_.logPhoneHubPermissionSetUpScreenAction(
        this.setupScreen_, PhoneHubPermissionsSetupAction.CANCEL);
    this.$.dialog.close();
  }

  private onDoneOrCloseButtonClicked_(): void {
    this.browserProxy_.logPhoneHubPermissionSetUpScreenAction(
        this.setupScreen_, PhoneHubPermissionsSetupAction.DONE);
    this.$.dialog.close();
  }

  private onLearnMoreClicked_(): void {
    this.browserProxy_.logPhoneHubPermissionSetUpScreenAction(
        this.setupScreen_, PhoneHubPermissionsSetupAction.LEARN_MORE);
    window.open(this.i18n('multidevicePhoneHubPermissionsLearnMoreURL'));
  }

  private onPinNumberSelected_(e: CustomEvent<{isPinNumberSelected: boolean}>):
      void {
    e.stopPropagation();
    assert(typeof e.detail.isPinNumberSelected === 'boolean');
    this.isPinNumberSelected_ = e.detail.isPinNumberSelected;
  }

  private onSetPinDone_(): void {
    // Once users confirm pin number, take them to the 'finish setup on the
    // phone' step directly.
    this.isPinSet_ = true;
    this.nextPage_();
  }

  private propagatePinNumberSelected_(selected: boolean): void {
    const pinNumberEvent = new CustomEvent('pin-number-selected', {
      bubbles: true,
      composed: true,
      detail: {isPinNumberSelected: selected},
    });
    this.dispatchEvent(pinNumberEvent);
  }

  private getCurrentScreen_(): PhoneHubPermissionsSetupFlowScreens {
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
      case Status.COMPLETED_USER_REJECTED:
      case Status.FAILED_OR_CANCELLED:
      case Status.CAMERA_ROLL_GRANTED_NOTIFICATION_REJECTED:
      case Status.CAMERA_ROLL_REJECTED_NOTIFICATION_GRANTED:
        return PhoneHubPermissionsSetupFlowScreens.CONNECTED;
      case Status.TIMED_OUT_CONNECTING:
        return PhoneHubPermissionsSetupFlowScreens.CONNECTION_TIME_OUT;
      case Status.CONNECTION_DISCONNECTED:
        return PhoneHubPermissionsSetupFlowScreens.CONNECTION_ERROR;
      default:
        return PhoneHubPermissionsSetupFlowScreens.NOT_APPLICABLE;
    }
  }

  private getTitle_(): string {
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
      case Status.COMPLETED_SUCCESSFULLY:
      case Status.COMPLETED_USER_REJECTED:
      case Status.FAILED_OR_CANCELLED:
      case Status.CAMERA_ROLL_GRANTED_NOTIFICATION_REJECTED:
      case Status.CAMERA_ROLL_REJECTED_NOTIFICATION_GRANTED:
        return this.getSetupCompleteTitle_();
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
  }

  /**
   * @return A description about the connection attempt state.
   */
  private getDescription_(): TrustedHTML|string {
    if (this.flowState_ === SetupFlowStatus.INTRO) {
      return '';
    }

    if (this.flowState_ === SetupFlowStatus.SET_LOCKSCREEN) {
      return '';
    }

    const Status = PermissionsSetupStatus;
    switch (this.setupState_) {
      case Status.COMPLETED_USER_REJECTED:
      case Status.FAILED_OR_CANCELLED:
        return (this.completedMode_ === 0) ?
            '' :
            this.i18n(
                'multidevicePermissionsSetupCompletedMoreFeaturesSummary');
      case Status.COMPLETED_SUCCESSFULLY:
      case Status.CAMERA_ROLL_GRANTED_NOTIFICATION_REJECTED:
      case Status.CAMERA_ROLL_REJECTED_NOTIFICATION_GRANTED:
        return (this.setupMode_ === this.completedMode_) ?
            '' :
            this.i18n(
                'multidevicePermissionsSetupCompletedMoreFeaturesSummary');
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
        return this.i18n('multidevicePermissionsSetupInstructions');
      default:
        return '';
    }
  }

  private getLiveStatus_(): string {
    // Because the title is dynamically changed on the single dialog, there
    // are cases when the "Connecting" screen is visually skipped in the
    // multidevice permissions dialog because the phone is already
    // connected, however, the Chromevox gets "stuck" on the Connecting
    // screen and reads it. This work around adds attributes to get the
    // "Finish setting up on your phone" title and description to fire in
    // Chromevox.
    //
    // TODO(b/293308787): Investigate potential solutions to improve this,
    // such as - if the phone is already connected, skip the "Connecting"
    // screen all together to prevent this issue.
    if (this.setupState_ ===
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE) {
      return 'polite';
    }

    return 'off';
  }

  private computeShouldShowLearnMoreButton_(): boolean {
    return this.flowState_ === SetupFlowStatus.INTRO ||
        this.flowState_ === SetupFlowStatus.SET_LOCKSCREEN ||
        this.setupState_ ===
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE;
  }

  private shouldShowCancelButton_(): boolean {
    return this.setupState_ !== PermissionsSetupStatus.COMPLETED_SUCCESSFULLY &&
        this.setupState_ !==
            PermissionsSetupStatus.NOTIFICATION_ACCESS_PROHIBITED &&
        this.setupState_ !==
            PermissionsSetupStatus.CAMERA_ROLL_GRANTED_NOTIFICATION_REJECTED &&
        this.setupState_ !==
            PermissionsSetupStatus.CAMERA_ROLL_REJECTED_NOTIFICATION_GRANTED &&
        this.setupState_ !== PermissionsSetupStatus.COMPLETED_USER_REJECTED &&
        this.setupState_ !== PermissionsSetupStatus.FAILED_OR_CANCELLED ||
        this.noFeatureHasBeenSetupWhenCompleted_();
  }

  private computeShouldShowDisabledDoneButton_(): boolean {
    return this.setupState_ ===
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE;
  }

  private shouldShowTryAgainButton_(): boolean {
    return this.setupState_ === PermissionsSetupStatus.TIMED_OUT_CONNECTING ||
        this.setupState_ === PermissionsSetupStatus.CONNECTION_DISCONNECTED ||
        this.noFeatureHasBeenSetupWhenCompleted_();
  }

  private shouldShowScreenLockInstructions_(): boolean {
    return this.flowState_ === SetupFlowStatus.SET_LOCKSCREEN;
  }

  private isScreenLockRequired_(): boolean {
    return loadTimeData.getBoolean('isEcheAppEnabled') &&
        this.isPhoneScreenLockEnabled && !this.isChromeosScreenLockEnabled &&
        this.showAppStreaming;
  }

  private getLearnMoreButtonAriaLabel_(): string {
    return this.i18n('multidevicePhoneHubLearnMoreAriaLabel');
  }

  private getSetupCompleteTitle_(): string {
    switch (this.completedMode_) {
      case NOTIFICATION_FEATURE:
        return this.i18n(
            'multidevicePermissionsSetupNotificationsCompletedTitle');
      case CAMERA_ROLL_FEATURE:
        return this.i18n('multidevicePermissionsSetupCameraRollCompletedTitle');
      case NOTIFICATION_FEATURE|CAMERA_ROLL_FEATURE:
        return this.i18n(
            'multidevicePermissionsSetupCameraRollAndNotificationsCompletedTitle');
      case APPS_FEATURE:
        return this.i18n('multidevicePermissionsSetupAppssCompletedTitle');
      case NOTIFICATION_FEATURE|APPS_FEATURE:
        return this.i18n(
            'multidevicePermissionsSetupNotificationsAndAppsCompletedTitle');
      case CAMERA_ROLL_FEATURE|APPS_FEATURE:
        return this.i18n(
            'multidevicePermissionsSetupCameraRollAndAppsCompletedTitle');
      case NOTIFICATION_FEATURE|CAMERA_ROLL_FEATURE|APPS_FEATURE:
        return this.i18n('multidevicePermissionsSetupAllCompletedTitle');
      default:
        return this.i18n(
            'multidevicePermissionsSetupAppssCompletedFailedTitle');
    }
  }

  private computePhoneHubPermissionsSetupMode_(
      mode: PhoneHubPermissionsSetupFeatureCombination):
      PhoneHubPermissionsSetupFeatureCombination {
    switch (mode) {
      case NOTIFICATION_FEATURE:
        return PhoneHubPermissionsSetupFeatureCombination.NOTIFICATION;
      case CAMERA_ROLL_FEATURE:
        return PhoneHubPermissionsSetupFeatureCombination.CAMERA_ROLL;
      case NOTIFICATION_FEATURE|CAMERA_ROLL_FEATURE:
        return PhoneHubPermissionsSetupFeatureCombination
            .NOTIFICATION_AND_CAMERA_ROLL;
      case APPS_FEATURE:
        return PhoneHubPermissionsSetupFeatureCombination.MESSAGING_APP;
      case NOTIFICATION_FEATURE|APPS_FEATURE:
        return PhoneHubPermissionsSetupFeatureCombination
            .NOTIFICATION_AND_MESSAGING_APP;
      case CAMERA_ROLL_FEATURE|APPS_FEATURE:
        return PhoneHubPermissionsSetupFeatureCombination
            .MESSAGING_APP_AND_CAMERA_ROLL;
      case NOTIFICATION_FEATURE|CAMERA_ROLL_FEATURE|APPS_FEATURE:
        return PhoneHubPermissionsSetupFeatureCombination.ALL_PERMISSONS;
      default:
        return PhoneHubPermissionsSetupFeatureCombination.NONE;
    }
  }

  private onAccessStateChanged_(): void {
    if (this.flowState_ === SetupFlowStatus.INTRO && !this.showCameraRoll &&
        !this.showNotifications && !this.showAppStreaming) {
      this.$.dialog.close();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsMultidevicePermissionsSetupDialogElement.is]:
        SettingsMultidevicePermissionsSetupDialogElement;
  }
  interface HTMLElementEventMap {
    'pin-number-selected': CustomEvent<{isPinNumberSelected: boolean}>;
  }
}

customElements.define(
    SettingsMultidevicePermissionsSetupDialogElement.is,
    SettingsMultidevicePermissionsSetupDialogElement);
