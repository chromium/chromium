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

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {sanitizeInnerHtml} from '//resources/ash/common/parse_html_subset.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';

import {CheckingDownloadingUpdate} from './checking_downloading_update.js';


/**
 * Possible UI states of the screen. Must be in the same order as
 * UpdateRequiredView::UIState enum values.
 * @enum {string}
 */
const UpdateRequiredUIState = {
  UPDATE_REQUIRED_MESSAGE: 'update-required-message',
  UPDATE_PROCESS: 'update-process',
  UPDATE_NEED_PERMISSION: 'update-need-permission',
  UPDATE_COMPLETED_NEED_REBOOT: 'update-completed-need-reboot',
  UPDATE_ERROR: 'update-error',
  EOL_REACHED: 'eol',
  UPDATE_NO_NETWORK: 'update-no-network',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const UpdateRequiredBase = mixinBehaviors(
    [OobeI18nBehavior, MultiStepBehavior, LoginScreenBehavior], PolymerElement);

/**
 * @typedef {{
 *   confirmationDialog: OobeModalDialog,
 *   downloadingUpdate: CheckingDownloadingUpdate,
 * }}
 */
UpdateRequiredBase.$;

/**
 * @polymer
 */
class UpdateRequired extends UpdateRequiredBase {
  static get is() {
    return 'update-required-card-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Is device connected to network?
       */
      isNetworkConnected: {type: Boolean, value: false},

      updateProgressUnavailable: {type: Boolean, value: true},

      updateProgressValue: {type: Number, value: 0},

      updateProgressMessage: {type: String, value: ''},

      estimatedTimeLeftVisible: {type: Boolean, value: false},

      enterpriseManager: {type: String, value: ''},

      deviceName: {type: String, value: ''},

      eolAdminMessage_: {type: String, value: ''},

      usersDataPresent_: {type: Boolean, value: false},

      /**
       * Estimated time left in seconds.
       */
      estimatedTimeLeft: {
        type: Number,
        value: 0,
      },
    };
  }

  /** Overridden from LoginScreenBehavior. */
  // clang-format off
  get EXTERNAL_API() {
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
  // clang-format on

  ready() {
    super.ready();
    this.initializeLoginScreen('UpdateRequiredScreen');
    this.updateEolDeleteUsersDataMessage_();
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.BLOCKING;
  }

  defaultUIStep() {
    return UpdateRequiredUIState.UPDATE_REQUIRED_MESSAGE;
  }

  get UI_STEPS() {
    return UpdateRequiredUIState;
  }

  onBeforeShow() {
    this.$.downloadingUpdate.onBeforeShow();
  }

  /** Called after resources are updated. */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
    this.updateEolDeleteUsersDataMessage_();
  }

  /**
   * @param {string} enterpriseManager Manager of device -could be a domain
   *    name or an email address.
   */
  /** @param {string} device Device name */
  setEnterpriseAndDeviceName(enterpriseManager, device) {
    this.enterpriseManager = enterpriseManager;
    this.deviceName = device;
  }

  /**
   * @param {string} eolMessage Not sanitized end of life message from policy
   */
  setEolMessage(eolMessage) {
    this.eolAdminMessage_ = sanitizeInnerHtml(eolMessage).toString();
  }

  /** @param {boolean} connected */
  setIsConnected(connected) {
    this.isNetworkConnected = connected;
  }

  /**
   * @param {boolean} unavailable
   */
  setUpdateProgressUnavailable(unavailable) {
    this.updateProgressUnavailable = unavailable;
  }

  /**
   * Sets update's progress bar value.
   * @param {number} progress Percentage of the progress bar.
   */
  setUpdateProgressValue(progress) {
    this.updateProgressValue = progress;
  }

  /**
   * Sets message below progress bar.
   * @param {string} message Message that should be shown.
   */
  setUpdateProgressMessage(message) {
    this.updateProgressMessage = message;
  }

  /**
   * Shows or hides downloading ETA message.
   * @param {boolean} visible Are ETA message visible?
   */
  setEstimatedTimeLeftVisible(visible) {
    this.estimatedTimeLeftVisible = visible;
  }

  /**
   * Sets estimated time left until download will complete.
   * @param {number} seconds Time left in seconds.
   */
  setEstimatedTimeLeft(seconds) {
    this.estimatedTimeLeft = seconds;
  }

  /**
   * Sets current UI state of the screen.
   * @param {number} ui_state New UI state of the screen.
   */
  setUIState(ui_state) {
    this.setUIStep(Object.values(UpdateRequiredUIState)[ui_state]);
  }

  /** @param {boolean} data_present */
  setIsUserDataPresent(data_present) {
    this.usersDataPresent_ = data_present;
  }

  /**
   * @private
   */
  onSelectNetworkClicked_() {
    this.userActed('select-network');
  }

  /**
   * @private
   */
  onUpdateClicked_() {
    this.userActed('update');
  }

  /**
   * @private
   */
  onFinishClicked_() {
    this.userActed('finish');
  }

  /**
   * @private
   */
  onCellularPermissionRejected_() {
    this.userActed('update-reject-cellular');
  }

  /**
   * @private
   */
  onCellularPermissionAccepted_() {
    this.userActed('update-accept-cellular');
  }

  /**
   * Simple equality comparison function.
   * @private
   */
  eq_(one, another) {
    return one === another;
  }

  /**
   * @private
   */
  isEmpty_(eolAdminMessage) {
    return !eolAdminMessage || eolAdminMessage.trim().length == 0;
  }

  /**
   * @private
   */
  updateEolDeleteUsersDataMessage_() {
    this.shadowRoot.querySelector('#deleteUsersDataMessage').innerHTML =
        this.i18nAdvanced('eolDeleteUsersDataMessage', {
          substitutions: [loadTimeData.getString('deviceType')],
          attrs: ['id'],
        });
    const linkElement = this.shadowRoot.querySelector('#deleteDataLink');
    linkElement.setAttribute('is', 'action-link');
    linkElement.classList.add('oobe-local-link');
    linkElement.addEventListener('click', () => this.showConfirmationDialog_());
  }

  /**
   * @private
   */
  showConfirmationDialog_() {
    this.$.confirmationDialog.showDialog();
  }

  /**
   * @private
   */
  hideConfirmationDialog_() {
    this.$.confirmationDialog.hideDialog();
  }

  /**
   * @private
   */
  onDeleteUsersConfirmed_() {
    this.userActed('confirm-delete-users');
    this.hideConfirmationDialog_();
  }
}

customElements.define(UpdateRequired.is, UpdateRequired);
