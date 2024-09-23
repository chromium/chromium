// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Terms Of Service
 * screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './managed_terms_of_service.html.js';


// Enum that describes the current state of the Terms Of Service screen
enum ManagedTermsState {
  LOADING = 'loading',
  LOADED = 'loaded',
  ERROR = 'error',
}

const ManagedTermsOfServiceBase = OobeDialogHostMixin(
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement))));

interface ManagedTermsOfServiceScreenData {
  manager: string;
}

export class ManagedTermsOfService extends ManagedTermsOfServiceBase {
  static get is() {
    return 'managed-terms-of-service-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      // Whether the back button is disabled.
      backButtonDisabled: {type: Boolean, value: false},

      // Whether the retry button is disabled.
      retryButtonDisabled: {type: Boolean, value: true},

      // Whether the accept button is disabled.
      acceptButtonDisabled: {type: Boolean, value: true},

      // The manager that the terms of service belongs to.
      tosManager: {type: String, value: ''},
    };
  }

  private backButtonDisabled: boolean;
  private retryButtonDisabled: boolean;
  private acceptButtonDisabled: boolean;
  private tosManager: string;

  constructor() {
    super();
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return ManagedTermsState.LOADING;
  }

  override get UI_STEPS() {
    return ManagedTermsState;
  }

  override get EXTERNAL_API() {
    return ['setTermsOfServiceLoadError', 'setTermsOfService'];
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * data contains manager string whose Terms of Service are being shown.
   */
  override onBeforeShow(data: ManagedTermsOfServiceScreenData): void {
    super.onBeforeShow(data);
    this.tosManager = data.manager;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('TermsOfServiceScreen');
  }

  /**
   * The 'on-tap' event handler for the 'Accept' button.
   */
  private onTermsOfServiceAccepted(): void {
    this.backButtonDisabled = true;
    this.acceptButtonDisabled = true;
    this.userActed('accept');
  }

  /**
   * The 'on-tap' event handler for the 'Back' button.
   */
  private onTosBackButtonPressed(): void {
    this.backButtonDisabled = true;
    this.retryButtonDisabled = true;
    this.acceptButtonDisabled = true;
    this.userActed('back');
  }

  /**
   * The 'on-tap' event handler for the 'Back' button.
   */
  private onTosRetryButtonPressed(): void {
    this.retryButtonDisabled = true;
    this.userActed('retry');
    // Show loading state while retrying.
    this.setUIStep(ManagedTermsState.LOADING);
  }

  /**
   * Displays an error message on the Terms of Service screen. Called when the
   * download of the Terms of Service has failed.
   */
  private setTermsOfServiceLoadError(): void {
    // Disable the accept button, hide the iframe, show warning icon and retry
    // button.
    this.setUIStep(ManagedTermsState.ERROR);

    this.acceptButtonDisabled = true;
    this.backButtonDisabled = false;
    this.retryButtonDisabled = false;
  }

  /**
   * Displays the given |termsOfService| and enables the accept button.
     termsOfService The terms of service, as plain text.
   */
  private setTermsOfService(termsOfService: string): void {
    const webview = this.shadowRoot!.getElementById('termsOfServiceFrame')! as
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
            '#tosContainer {' +
            '  overflow: auto;' +
            '  height: 99%;' +
            '  padding-left: 16px;' +
            '  padding-right: 16px;' +
            '}' +
            '#tosContainer::-webkit-scrollbar-thumb {' +
            '  border-radius: 10px;' +
            '}' +
            '</style>' +
            '<body><div id="tosContainer">' + termsOfService + '</div>' +
            '</body>');

    // Mark the loading as complete.
    this.acceptButtonDisabled = false;
    this.setUIStep(ManagedTermsState.LOADED);
  }

  /**
   * Generates fonts styling for the ToS WebView based on OobeJelly flag.
   */
  private getServiceLogsFontsStyling(): string {
    const isOobeJellyEnabled = loadTimeData.getBoolean('isOobeJellyEnabled');
    if (!isOobeJellyEnabled) {
      return '  font-family: Roboto, sans-serif;' +
          '  font-size: 14sp;';
    }
    // Those values correspond to the cros-body-2 token.
    return (
        '  font-family: Google Sans Text Regular, Google Sans, Roboto, sans-serif;' +
        '  font-size: 13px;' +
        '  font-weight: 400;' +
        '  line-height: 20px;');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ManagedTermsOfService.is]: ManagedTermsOfService;
  }
}

customElements.define(ManagedTermsOfService.is, ManagedTermsOfService);
