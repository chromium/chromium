// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Polymer element for displaying material design update required.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {sanitizeInnerHtml} from '//resources/js/parse_html_subset.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {CheckingDownloadingUpdate} from './checking_downloading_update.js';
import {getTemplate} from './update_required_card.html.js';


/**
 * Possible UI states of the screen. Must be in the same order as
 * UpdateRequiredView::UIState enum values.
 */
enum UpdateRequiredUiState {
  UPDATE_REQUIRED_MESSAGE = 'update-required-message',
  UPDATE_PROCESS = 'update-process',
  UPDATE_NEED_PERMISSION = 'update-need-permission',
  UPDATE_COMPLETED_NEED_REBOOT = 'update-completed-need-reboot',
  UPDATE_ERROR = 'update-error',
  EOL_REACHED = 'eol',
  UPDATE_NO_NETWORK = 'update-no-network',
}

const UpdateRequiredBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

export class UpdateRequired extends UpdateRequiredBase {
  static get is() {
    return 'update-required-card-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Is device connected to network?
       */
      isNetworkConnected: {
        type: Boolean,
        value: false,
      },

      updateProgressUnavailable: {
        type: Boolean,
        value: true,
      },

      updateProgressValue: {
        type: Number,
        value: 0,
      },

      updateProgressMessage: {
        type: String,
        value: '',
      },

      estimatedTimeLeftVisible: {
        type: Boolean,
        value: false,
      },

      enterpriseManager: {
        type: String,
        value: '',
      },

      deviceName: {
        type: String,
        value: '',
      },

      eolAdminMessage: {
        type: String,
        value: '',
      },

      usersDataPresent: {
        type: Boolean,
        value: false,
      },

      /**
       * Estimated time left in seconds.
       */
      estimatedTimeLeft: {
        type: Number,
        value: 0,
      },
    };
  }

  private isNetworkConnected: boolean;
  private updateProgressUnavailable: boolean;
  private updateProgressValue: number;
  private updateProgressMessage: string;
  private estimatedTimeLeftVisible: boolean;
  private enterpriseManager: string;
  private deviceName: string;
  private eolAdminMessage: string;
  private usersDataPresent: boolean;
  private estimatedTimeLeft: number;

  override get EXTERNAL_API(): string[] {
    return [
      'setIsConnected',
      'setUpdateProgressUnavailable',
      'setUpdateProgressValue',
      'setUpdateProgressMessage',
      'setEstimatedTimeLeftVisible',
      'setEstimatedTimeLeft',
      'setUIState',
      'setEnterpriseAndDeviceName',
      'setEolMessage',
      'setIsUserDataPresent',
    ];
  }

  override ready() {
    super.ready();
    this.initializeLoginScreen('UpdateRequiredScreen');
    this.updateEolDeleteUsersDataMessage();
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.BLOCKING;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return UpdateRequiredUiState.UPDATE_REQUIRED_MESSAGE;
  }

  override get UI_STEPS() {
    return UpdateRequiredUiState;
  }

  override onBeforeShow(): void {
    super.onBeforeShow();
    const elem = this.shadowRoot?.querySelector('#downloadingUpdate');
    if (elem instanceof CheckingDownloadingUpdate) {
      elem.onBeforeShow();
    }
  }

  /** Called after resources are updated. */
  override updateLocalizedContent(): void {
    this.i18nUpdateLocale();
    this.updateEolDeleteUsersDataMessage();
  }

  /**
   * @param enterpriseManager Manager of device -could be a domain
   *    name or an email address.
   */
  setEnterpriseAndDeviceName(enterpriseManager: string, device: string): void {
    this.enterpriseManager = enterpriseManager;
    this.deviceName = device;
  }

  /**
   * @param eolMessage Not sanitized end of life message from policy
   */
  setEolMessage(eolMessage: string): void {
    this.eolAdminMessage = sanitizeInnerHtml(eolMessage).toString();
  }

  setIsConnected(connected: boolean): void {
    this.isNetworkConnected = connected;
  }

  /**
   */
  setUpdateProgressUnavailable(unavailable: boolean): void {
    this.updateProgressUnavailable = unavailable;
  }

  /**
   * Sets update's progress bar value.
   * @param progress Percentage of the progress bar.
   */
  setUpdateProgressValue(progress: number): void {
    this.updateProgressValue = progress;
  }

  /**
   * Sets message below progress bar.
   * @param message Message that should be shown.
   */
  setUpdateProgressMessage(message: string): void {
    this.updateProgressMessage = message;
  }

  /**
   * Shows or hides downloading ETA message.
   * @param visible Are ETA message visible?
   */
  setEstimatedTimeLeftVisible(visible: boolean): void {
    this.estimatedTimeLeftVisible = visible;
  }

  /**
   * Sets estimated time left until download will complete.
   * @param seconds Time left in seconds.
   */
  setEstimatedTimeLeft(seconds: number): void {
    this.estimatedTimeLeft = seconds;
  }

  /**
   * Sets current UI state of the screen.
   */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  setUIState(uiState: number): void {
    this.setUIStep(Object.values(UpdateRequiredUiState)[uiState]);
  }

  setIsUserDataPresent(dataPresent: boolean): void {
    this.usersDataPresent = dataPresent;
  }

  private onSelectNetworkClicked(): void {
    this.userActed('select-network');
  }

  private onUpdateClicked(): void {
    this.userActed('update');
  }

  private onFinishClicked(): void {
    this.userActed('finish');
  }

  private onCellularPermissionRejected(): void {
    this.userActed('update-reject-cellular');
  }

  private onCellularPermissionAccepted(): void {
    this.userActed('update-accept-cellular');
  }

  /**
   * Simple equality comparison function.
   */
  private eq(one: any, another: any): boolean {
    return one === another;
  }

  private isEmpty(eolAdminMessage: string): boolean {
    return !eolAdminMessage || eolAdminMessage.trim().length === 0;
  }

  private updateEolDeleteUsersDataMessage(): void {
    const message = this.shadowRoot?.querySelector('#deleteUsersDataMessage');
    if (message instanceof HTMLElement) {
      message.innerHTML = this.i18nAdvanced('eolDeleteUsersDataMessage', {
        substitutions: [loadTimeData.getString('deviceType')],
        attrs: ['id'],
      });
    }
    const linkElement = this.shadowRoot?.querySelector('#deleteDataLink')!;
    if (linkElement instanceof HTMLAnchorElement) {
      linkElement.setAttribute('is', 'action-link');
      linkElement.setAttribute('aria-describedby', 'deleteUsersDataMessage');
      linkElement.classList.add('oobe-local-link');
      linkElement.addEventListener(
          'click', () => this.showConfirmationDialog());
    }
  }

  private showConfirmationDialog(): void {
    const dialog = this.shadowRoot?.querySelector('#confirmationDialog');
    if (dialog instanceof OobeModalDialog) {
      dialog.showDialog();
    }
  }

  private hideConfirmationDialog(): void {
    const dialog = this.shadowRoot?.querySelector('#confirmationDialog');
    if (dialog instanceof OobeModalDialog) {
      dialog.hideDialog();
    }
  }

  private onDeleteUsersConfirmed(): void {
    this.userActed('confirm-delete-users');
    this.hideConfirmationDialog();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [UpdateRequired.is]: UpdateRequired;
  }
}

customElements.define(UpdateRequired.is, UpdateRequired);
