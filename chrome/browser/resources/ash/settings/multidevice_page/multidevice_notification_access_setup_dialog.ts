// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This element provides the Phone Hub notification access setup flow that, when
 * successfully completed, enables the feature that allows a user's phone
 * notifications to be mirrored on their Chromebook.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import '../os_settings_icons.html.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from './multidevice_browser_proxy.js';
import {MultiDeviceFeature} from './multidevice_constants.js';
import {getTemplate} from './multidevice_notification_access_setup_dialog.html.js';

/**
 * Numerical values should not be changed because they must stay in sync with
 * notification_access_setup_operation.h, with the exception of
 * CONNECTION_REQUESTED.
 */
export enum NotificationAccessSetupOperationStatus {
  CONNECTION_REQUESTED = 0,
  CONNECTING = 1,
  TIMED_OUT_CONNECTING = 2,
  CONNECTION_DISCONNECTED = 3,
  SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE = 4,
  COMPLETED_SUCCESSFULLY = 5,
  NOTIFICATION_ACCESS_PROHIBITED = 6,
}

export interface SettingsMultideviceNotificationAccessSetupDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const SettingsMultideviceNotificationAccessSetupDialogElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class SettingsMultideviceNotificationAccessSetupDialogElement extends
    SettingsMultideviceNotificationAccessSetupDialogElementBase {
  static get is() {
    return 'settings-multidevice-notification-access-setup-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * A null |setupState_| indicates that the operation has not yet started.
       */
      setupState_: {
        type: Number,
        value: null,
      },

      title_: {
        type: String,
        computed: 'getTitle_(setupState_)',
      },

      description_: {
        type: String,
        computed: 'getDescription_(setupState_)',
      },

      hasNotStartedSetupAttempt_: {
        type: Boolean,
        computed: 'computeHasNotStartedSetupAttempt_(setupState_)',
        reflectToAttribute: true,
      },

      isSetupAttemptInProgress_: {
        type: Boolean,
        computed: 'computeIsSetupAttemptInProgress_(setupState_)',
        reflectToAttribute: true,
      },

      didSetupAttemptFail_: {
        type: Boolean,
        computed: 'computeDidSetupAttemptFail_(setupState_)',
        reflectToAttribute: true,
      },

      hasCompletedSetupSuccessfully_: {
        type: Boolean,
        computed: 'computeHasCompletedSetupSuccessfully_(setupState_)',
        reflectToAttribute: true,
      },

      isNotificationAccessProhibited_: {
        type: Boolean,
        computed: 'computeIsNotificationAccessProhibited_(setupState_)',
      },

      shouldShowSetupInstructionsSeparately_: {
        type: Boolean,
        computed: 'computeShouldShowSetupInstructionsSeparately_(' +
            'setupState_)',
        reflectToAttribute: true,
      },
    };
  }

  private browserProxy_: MultiDeviceBrowserProxy;
  private description_: string;
  private didSetupAttemptFail_: boolean;
  private hasCompletedSetupSuccessfully_: boolean;
  private hasNotStartedSetupAttempt_: boolean;
  private isNotificationAccessProhibited_: boolean;
  private isSetupAttemptInProgress_: boolean;
  private setupState_: NotificationAccessSetupOperationStatus|null;
  private shouldShowSetupInstructionsSeparately_: boolean;
  private title_: string;

  constructor() {
    super();

    this.browserProxy_ = MultiDeviceBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.addWebUiListener(
        'settings.onNotificationAccessSetupStatusChanged',
        this.onSetupStateChanged_.bind(this));
    this.$.dialog.showModal();
  }

  private onSetupStateChanged_(
      setupState: NotificationAccessSetupOperationStatus): void {
    this.setupState_ = setupState;
    if (this.setupState_ ===
        NotificationAccessSetupOperationStatus.COMPLETED_SUCCESSFULLY) {
      this.browserProxy_.setFeatureEnabledState(
          MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS, true);
    }
  }

  private computeHasNotStartedSetupAttempt_(): boolean {
    return this.setupState_ === null;
  }

  private computeIsSetupAttemptInProgress_(): boolean {
    return this.setupState_ ===
        NotificationAccessSetupOperationStatus
            .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE ||
        this.setupState_ ===
        NotificationAccessSetupOperationStatus.CONNECTING ||
        this.setupState_ ===
        NotificationAccessSetupOperationStatus.CONNECTION_REQUESTED;
  }

  private computeHasCompletedSetupSuccessfully_(): boolean {
    return this.setupState_ ===
        NotificationAccessSetupOperationStatus.COMPLETED_SUCCESSFULLY;
  }

  private computeIsNotificationAccessProhibited_(): boolean {
    return this.setupState_ ===
        NotificationAccessSetupOperationStatus.NOTIFICATION_ACCESS_PROHIBITED;
  }

  private computeDidSetupAttemptFail_(): boolean {
    return this.setupState_ ===
        NotificationAccessSetupOperationStatus.TIMED_OUT_CONNECTING ||
        this.setupState_ ===
        NotificationAccessSetupOperationStatus.CONNECTION_DISCONNECTED ||
        this.setupState_ ===
        NotificationAccessSetupOperationStatus.NOTIFICATION_ACCESS_PROHIBITED;
  }

  /**
   * @return Whether to show setup instructions in its own section.
   */
  private computeShouldShowSetupInstructionsSeparately_(): boolean {
    return this.setupState_ === null ||
        this.setupState_ ===
        NotificationAccessSetupOperationStatus.CONNECTION_REQUESTED ||
        this.setupState_ ===
        NotificationAccessSetupOperationStatus
            .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE ||
        this.setupState_ === NotificationAccessSetupOperationStatus.CONNECTING;
  }

  private attemptNotificationSetup_(): void {
    this.browserProxy_.attemptNotificationSetup();
    this.setupState_ =
        NotificationAccessSetupOperationStatus.CONNECTION_REQUESTED;
  }

  private onCancelClicked_(): void {
    this.browserProxy_.cancelNotificationSetup();
    this.$.dialog.close();
  }

  private onDoneOrCloseButtonClicked_(): void {
    this.$.dialog.close();
  }

  private getTitle_(): string {
    if (this.setupState_ === null) {
      return this.i18n('multideviceNotificationAccessSetupAckTitle');
    }

    const Status = NotificationAccessSetupOperationStatus;
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
  }

  /**
   * @return A description about the connection attempt state.
   */
  private getDescription_(): TrustedHTML|string {
    if (this.setupState_ === null) {
      return this.i18n('multideviceNotificationAccessSetupAckSummary');
    }

    const Status = NotificationAccessSetupOperationStatus;
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
  }

  private shouldShowCancelButton_(): boolean {
    return this.setupState_ !==
        NotificationAccessSetupOperationStatus.COMPLETED_SUCCESSFULLY &&
        this.setupState_ !==
        NotificationAccessSetupOperationStatus.NOTIFICATION_ACCESS_PROHIBITED;
  }

  private shouldShowTryAgainButton_(): boolean {
    return this.setupState_ ===
        NotificationAccessSetupOperationStatus.TIMED_OUT_CONNECTING ||
        this.setupState_ ===
        NotificationAccessSetupOperationStatus.CONNECTION_DISCONNECTED;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsMultideviceNotificationAccessSetupDialogElement.is]:
        SettingsMultideviceNotificationAccessSetupDialogElement;
  }
}

customElements.define(
    SettingsMultideviceNotificationAccessSetupDialogElement.is,
    SettingsMultideviceNotificationAccessSetupDialogElement);
