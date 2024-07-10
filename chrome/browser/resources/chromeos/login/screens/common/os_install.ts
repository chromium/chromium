// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for OS install screen.
 */

import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/dialogs/oobe_modal_dialog.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeTextButton} from '../../components/buttons/oobe_text_button.js';
import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './os_install.html.js';

enum OsInstallScreenSteps {
  INTRO = 'intro',
  IN_PROGRESS = 'in-progress',
  FAILED = 'failed',
  NO_DESTINATION_DEVICE_FOUND = 'no-destination-device-found',
  SUCCESS = 'success',
}


const OsInstallScreenElementBase = OobeDialogHostMixin(
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement))));

export class OsInstall extends OsInstallScreenElementBase {
  static get is() {
    return 'os-install-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Success step subtitile message.
       */
      osInstallDialogSuccessSubtitile: {
        type: String,
        value: '',
      },
    };
  }

  private osInstallDialogSuccessSubtitile: string;

  constructor() {
    super();
  }

  override get EXTERNAL_API(): string[] {
    return ['showStep', 'setServiceLogs', 'updateCountdownString'];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): OsInstallScreenSteps {
    return OsInstallScreenSteps.INTRO;
  }

  override get UI_STEPS() {
    return OsInstallScreenSteps;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('OsInstallScreen');
  }

  /**
   * Set and show screen step.
   */
  showStep(step: OsInstallScreenSteps): void {
    this.setUIStep(step);
  }

  /**
   * This is the 'on-click' event handler for the 'back' button.
   */
  private onBack(): void {
    this.userActed('os-install-exit');
  }

  private onIntroNextButtonPressed(): void {
    const confirmationDialog = this.shadowRoot?.querySelector<OobeModalDialog>(
        '#osInstallDialogConfirm');
    if (confirmationDialog instanceof OobeModalDialog) {
      confirmationDialog.showDialog();
    }
    const closeConfirmDialogButton =
        this.shadowRoot?.querySelector<OobeTextButton>(
            '#closeConfirmDialogButton');
    if (closeConfirmDialogButton instanceof OobeTextButton) {
      closeConfirmDialogButton.focus();
    }
  }

  private onConfirmNextButtonPressed(): void {
    const confirmationDialog = this.shadowRoot?.querySelector<OobeModalDialog>(
        '#osInstallDialogConfirm');
    if (confirmationDialog instanceof OobeModalDialog) {
      confirmationDialog.hideDialog();
    }
    this.userActed('os-install-confirm-next');
  }

  private onErrorSendFeedbackButtonPressed(): void {
    this.userActed('os-install-error-send-feedback');
  }

  private onErrorShutdownButtonPressed(): void {
    this.userActed('os-install-error-shutdown');
  }

  private onCloseConfirmDialogButtonPressed(): void {
    const confirmationDialog = this.shadowRoot?.querySelector<OobeModalDialog>(
        '#osInstallDialogConfirm');
    if (confirmationDialog instanceof OobeModalDialog) {
      confirmationDialog.hideDialog();
    }
    const osInstallIntroNextButton =
        this.shadowRoot?.querySelector<OobeTextButton>(
            '#osInstallIntroNextButton');
    if (osInstallIntroNextButton instanceof OobeTextButton) {
      osInstallIntroNextButton.focus();
    }
  }

  private getErrorNoDestContentHtml(locale: string): TrustedHTML {
    return this.i18nAdvancedDynamic(
        locale, 'osInstallDialogErrorNoDestContent', {
          tags: ['p', 'ul', 'li'],
        });
  }

  private getErrorFailedSubtitleHtml(locale: string): TrustedHTML {
    return this.i18nAdvancedDynamic(
        locale, 'osInstallDialogErrorFailedSubtitle', {
          tags: ['p'],
        });
  }

  /**
   * Shows service logs.
   */
  private onServiceLogsLinkClicked(): void {
    const serviceLogsDialog =
        this.shadowRoot?.querySelector<OobeModalDialog>('#serviceLogsDialog');
    if (serviceLogsDialog instanceof OobeModalDialog) {
      serviceLogsDialog.showDialog();
    }
    const closeServiceLogsDialog =
        this.shadowRoot?.querySelector<OobeTextButton>(
            '#closeServiceLogsDialog');
    if (closeServiceLogsDialog instanceof OobeTextButton) {
      closeServiceLogsDialog.focus();
    }
  }

  /**
   * On-click event handler for close button of the service logs dialog.
   */
  private hideServiceLogsDialog(): void {
    const serviceLogsDialog =
        this.shadowRoot?.querySelector<OobeModalDialog>('#serviceLogsDialog');
    if (serviceLogsDialog instanceof OobeModalDialog) {
      serviceLogsDialog.hideDialog();
    }
    this.focusLogsLink();
  }

  private focusLogsLink(): void {
    if (this.uiStep === OsInstallScreenSteps.NO_DESTINATION_DEVICE_FOUND) {
      afterNextRender(this, () => {
        const noDestLogsLink =
            this.shadowRoot?.querySelector<HTMLAnchorElement>(
                '#noDestLogsLink');
        if (noDestLogsLink instanceof HTMLAnchorElement) {
          noDestLogsLink.focus();
        }
      });
    } else if (this.uiStep === OsInstallScreenSteps.FAILED) {
      afterNextRender(this, () => {
        const serviceLogsLink =
            this.shadowRoot?.querySelector<HTMLAnchorElement>(
                '#serviceLogsLink');
        if (serviceLogsLink instanceof HTMLAnchorElement) {
          serviceLogsLink.focus();
        }
      });
    }
  }

  /**
   * serviceLogs Logs to show as plain text.
   */
  setServiceLogs(serviceLogs: string): void {
    const webview = this.shadowRoot!.getElementById('serviceLogsFrame')! as
        chrome.webviewTag.WebView;
    webview.src =
        'data:text/html;charset=utf-8,' +
        encodeURIComponent(
            '<style>' +
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
            '<body><div id="logsContainer">' + serviceLogs + '</div>' +
            '</body>');
  }

  /**
   * timeLeftMessage Countdown message on success step.
   */
  updateCountdownString(timeLeftMessage: string): void {
    this.osInstallDialogSuccessSubtitile = timeLeftMessage;
  }

  /**
   * Generates fonts styling for the service log WebView based on OobeJelly
   * flag.
   */
  private getServiceLogsFontsStyling(): string {
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

declare global {
  interface HTMLElementTagNameMap {
    [OsInstall.is]: OsInstall;
  }
}

customElements.define(OsInstall.is, OsInstall);
