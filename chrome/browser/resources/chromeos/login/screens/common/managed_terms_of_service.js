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

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';


// Enum that describes the current state of the Terms Of Service screen
const ManagedTermsState = {
  LOADING: 'loading',
  LOADED: 'loaded',
  ERROR: 'error',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const ManagedTermsOfServiceBase = mixinBehaviors(
    [
      OobeI18nBehavior,
      OobeDialogHostBehavior,
      LoginScreenBehavior,
      MultiStepBehavior,
    ],
    PolymerElement);

/**
 * @typedef {{
 *   termsOfServiceDialog:  OobeAdaptiveDialog,
 *   termsOfServiceFrame:  WebView,
 * }}
 */
ManagedTermsOfServiceBase.$;

/**
 * @polymer
 */
class ManagedTermsOfService extends ManagedTermsOfServiceBase {

  static get is() {
    return 'managed-terms-of-service-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      // Whether the back button is disabled.
      backButtonDisabled_: {type: Boolean, value: false},

      // Whether the retry button is disabled.
      retryButtonDisabled_: {type: Boolean, value: true},

      // Whether the accept button is disabled.
      acceptButtonDisabled_: {type: Boolean, value: true},

      // The manager that the terms of service belongs to.
      tosManager_: {type: String, value: ''},
    };
  }

  constructor() {
    super();
  }

  defaultUIStep() {
    return ManagedTermsState.LOADING;
  }

  get UI_STEPS() {
    return ManagedTermsState;
  }

  get EXTERNAL_API() {
    return ['setTermsOfServiceLoadError',
            'setTermsOfService'];
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {{manager: string}} data contains manager string whose
   * Terms of Service are being shown.
   */
  onBeforeShow(data) {
    this.tosManager_ = data.manager;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('TermsOfServiceScreen');
  }

  focus() {
    this.$.termsOfServiceDialog.show();
  }

  /**
   * The 'on-tap' event handler for the 'Accept' button.
   * @private
   */
  onTermsOfServiceAccepted_() {
    this.backButtonDisabled_ = true;
    this.acceptButtonDisabled_ = true;
    this.userActed('accept');
  }

  /**
   * The 'on-tap' event handler for the 'Back' button.
   * @private
   */
  onTosBackButtonPressed_() {
    this.backButtonDisabled_ = true;
    this.retryButtonDisabled_ = true;
    this.acceptButtonDisabled_ = true;
    this.userActed('back');
  }

  /**
   * The 'on-tap' event handler for the 'Back' button.
   * @private
   */
  onTosRetryButtonPressed_() {
    this.retryButtonDisabled_ = true;
    this.userActed('retry');
    // Show loading state while retrying.
    this.setUIStep(ManagedTermsState.LOADING);
  }

  /**
   * Displays an error message on the Terms of Service screen. Called when the
   * download of the Terms of Service has failed.
   */
  setTermsOfServiceLoadError() {
    // Disable the accept button, hide the iframe, show warning icon and retry
    // button.
    this.setUIStep(ManagedTermsState.ERROR);

    this.acceptButtonDisabled_ = true;
    this.backButtonDisabled_ = false;
    this.retryButtonDisabled_ = false;
  }

  /**
   * Displays the given |termsOfService| and enables the accept button.
   * @param {string} termsOfService The terms of service, as plain text.
   */
  setTermsOfService(termsOfService) {
    this.$.termsOfServiceFrame.src = 'data:text/html;charset=utf-8,' +
        encodeURIComponent('<style>' +
                           'body {' +
                           '  font-family: Roboto, sans-serif;' +
                           '  color: RGBA(0,0,0,.87);' +
                           '  font-size: 14sp;' +
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
                           '<body><div id="tosContainer">' + termsOfService +
                           '</div>' +
                           '</body>');

    // Mark the loading as complete.
    this.acceptButtonDisabled_ = false;
    this.setUIStep(ManagedTermsState.LOADED);
  }
}

customElements.define(ManagedTermsOfService.is, ManagedTermsOfService);
