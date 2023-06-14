// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for OS install screen.
 */

import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/dialogs/oobe_modal_dialog.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {afterNextRender, html, mixinBehaviors, Polymer, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';

const OsInstallScreenState = {
  INTRO: 'intro',
  IN_PROGRESS: 'in-progress',
  FAILED: 'failed',
  NO_DESTINATION_DEVICE_FOUND: 'no-destination-device-found',
  SUCCESS: 'success',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const OsInstallScreenElementBase = mixinBehaviors(
    [
      OobeI18nBehavior,
      OobeDialogHostBehavior,
      LoginScreenBehavior,
      MultiStepBehavior,
    ],
    PolymerElement);

/**
 * @polymer
 */
class OsInstall extends OsInstallScreenElementBase {
  static get is() {
    return 'os-install-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Success step subtitile message.
       */
      osInstallDialogSuccessSubtitile_: {
        type: String,
        value: '',
      },
    };
  }

  constructor() {
    super();
  }

  get EXTERNAL_API() {
    return ['showStep', 'setServiceLogs', 'updateCountdownString'];
  }

  defaultUIStep() {
    return OsInstallScreenState.INTRO;
  }

  get UI_STEPS() {
    return OsInstallScreenState;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('OsInstallScreen');
  }

  /**
   * Set and show screen step.
   * @param {string} step screen step.
   */
  showStep(step) {
    this.setUIStep(step);
  }

  /**
   * This is the 'on-click' event handler for the 'back' button.
   * @private
   */
  onBack_() {
    this.userActed('os-install-exit');
  }

  onIntroNextButtonPressed_() {
    this.$.osInstallDialogConfirm.showDialog();
    this.$.closeConfirmDialogButton.focus();
  }

  onConfirmNextButtonPressed_() {
    this.$.osInstallDialogConfirm.hideDialog();
    this.userActed('os-install-confirm-next');
  }

  onErrorSendFeedbackButtonPressed_() {
    this.userActed('os-install-error-send-feedback');
  }

  onErrorShutdownButtonPressed_() {
    this.userActed('os-install-error-shutdown');
  }

  onSuccessRestartButtonPressed_() {
    this.userActed('os-install-success-restart');
  }

  onCloseConfirmDialogButtonPressed_() {
    this.$.osInstallDialogConfirm.hideDialog();
    this.$.osInstallIntroNextButton.focus();
  }

  /**
   * @param {string} locale
   * @return {string}
   * @private
   */
  getErrorNoDestContentHtml_(locale) {
    return this.i18nAdvanced('osInstallDialogErrorNoDestContent', {
      tags: ['p', 'ul', 'li'],
    });
  }

  /**
   * @param {string} locale
   * @return {string}
   * @private
   */
  getErrorFailedSubtitleHtml_(locale) {
    return this.i18nAdvanced('osInstallDialogErrorFailedSubtitle', {
      tags: ['p'],
    });
  }

  /**
   * Shows service logs.
   * @private
   */
  onServiceLogsLinkClicked_() {
    this.$.serviceLogsDialog.showDialog();
    this.$.closeServiceLogsDialog.focus();
  }

  /**
   * On-click event handler for close button of the service logs dialog.
   * @private
   */
  hideServiceLogsDialog_() {
    this.$.serviceLogsDialog.hideDialog();
    this.focusLogsLink_();
  }

  /**
   * @private
   */
  focusLogsLink_() {
    if (this.uiStep == OsInstallScreenState.NO_DESTINATION_DEVICE_FOUND) {
      afterNextRender(this, () => this.$.noDestLogsLink.focus());
    } else if (this.uiStep == OsInstallScreenState.FAILED) {
      afterNextRender(this, () => this.$.serviceLogsLink.focus());
    }
  }

  /**
   * @param {string} serviceLogs Logs to show as plain text.
   */
  setServiceLogs(serviceLogs) {
    this.$.serviceLogsFrame.src = 'data:text/html;charset=utf-8,' +
        encodeURIComponent('<style>' +
                           'body {' + this.getServiceLogsFontsStyling() +
                           '  color: RGBA(0,0,0,.87);' +
                           '  margin : 0;' +
                           '  padding : 0;' +
                           '  white-space: pre-wrap;' +
                           '}' +
                           '#logsContainer {' +
                           '  overflow: auto;' +
                           '  height: 99%;' +
                           '  padding-left: 16px;' +
                           '  padding-right: 16px;' +
                           '}' +
                           '#logsContainer::-webkit-scrollbar-thumb {' +
                           '  border-radius: 10px;' +
                           '}' +
                           '</style>' +
                           '<body><div id="logsContainer">' + serviceLogs +
                           '</div>' +
                           '</body>');
  }

  /**
   * @param {string} timeLeftMessage Countdown message on success step.
   */
  updateCountdownString(timeLeftMessage) {
    this.osInstallDialogSuccessSubtitile_ = timeLeftMessage;
  }

  /**
   * Generates fonts styling for the service log WebView based on OobeJelly
   * flag.
   * @return {string}
   * @private
   */
  getServiceLogsFontsStyling() {
    const isOobeJellyEnabled = loadTimeData.getBoolean('isOobeJellyEnabled');
    if (!isOobeJellyEnabled) {
      return '  font-family: Roboto, sans-serif;' +
          '  font-size: 14sp;';
    }
    // Those values correspond to the cros-body-1 token.
    return (
        '  font-family: Google Sans Text Regular, Google Sans, Roboto, sans-serif;' +
        '  font-size: 14px;' +
        '  font-weight: 400;' +
        '  line-height: 20px;');
  }
}

customElements.define(OsInstall.is, OsInstall);
