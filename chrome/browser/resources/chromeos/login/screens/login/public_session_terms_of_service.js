// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {

// Enum that describes the current state of the Terms Of Service screen
const UIState = {
  LOADING: 'loading',
  LOADED: 'loaded',
  ERROR: 'error',
};

/**
 * @fileoverview Polymer element for displaying material design Terms Of Service
 * screen.
 */
Polymer({
  is: 'terms-of-service-element',

  behaviors: [
    OobeI18nBehavior,
    OobeDialogHostBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  properties: {

    // Whether the back button is disabled.
    backButtonDisabled_: {type: Boolean, value: false},

    // Whether the retry button is disabled.
    retryButtonDisabled_: {type: Boolean, value: true},

    // Whether the accept button is disabled.
    acceptButtonDisabled_: {type: Boolean, value: true},

    // The manager that the terms of service belongs to.
    tosManager_: {type: String, value: ''},
  },

  defaultUIStep() {
    return UIState.LOADING;
  },

  UI_STEPS: UIState,

  // Whether the screen is still loading.
  isLoading_() {
    return this.uiStep == UIState.LOADING;
  },

  // Whether the screen has finished loading.
  isLoaded_() {
    return this.uiStep == UIState.LOADED;
  },

  // Whether the screen is in an error state.
  hasError_() {
    return this.uiStep == UIState.ERROR;
  },

  EXTERNAL_API: [
    'setManager',
    'setTermsOfServiceLoadError',
    'setTermsOfService',
  ],

  /** @override */
  ready() {
    this.initializeLoginScreen('TermsOfServiceScreen', {
      resetAllowed: true,
    });
  },


  focus() {
    this.$.termsOfServiceDialog.show();
  },

  /**
   * This is called when strings are updated.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  /**
   * The 'on-tap' event handler for the 'Accept' button.
   * @private
   */
  onTermsOfServiceAccepted_() {
    // Ignore on-tap events when disabled.
    // TODO: Polymer Migration - Remove this when the migration is finished.
    // See: https://github.com/Polymer/polymer/issues/4685
    if (this.acceptButtonDisabled_)
      return;

    this.backButtonDisabled_ = true;
    this.acceptButtonDisabled_ = true;
    this.userActed('accept');
  },

  /**
   * The 'on-tap' event handler for the 'Back' button.
   * @private
   */
  onTosBackButtonPressed_() {
    // Ignore on-tap events when disabled.
    // TODO: Polymer Migration - Remove this when the migration is finished.
    // See: https://github.com/Polymer/polymer/issues/4685
    if (this.backButtonDisabled_)
      return;

    this.backButtonDisabled_ = true;
    this.retryButtonDisabled_ = true;
    this.acceptButtonDisabled_ = true;
    this.userActed('back');
  },

  /**
   * The 'on-tap' event handler for the 'Back' button.
   * @private
   */
  onTosRetryButtonPressed_() {
    // Ignore on-tap events when disabled.
    // TODO: Polymer Migration - Remove this when the migration is finished.
    // See: https://github.com/Polymer/polymer/issues/4685
    if (this.retryButtonDisabled_)
      return;

    this.retryButtonDisabled_ = true;
    this.userActed('retry');
  },

  /**
   * Updates headings on the screen to indicate that the Terms of Service
   * being shown belong to |manager|.
   * @param {string} manager The manager whose Terms of Service are being shown.
   */
  setManager(manager) {
    this.tosManager_ = manager;
  },

  /**
   * Displays an error message on the Terms of Service screen. Called when the
   * download of the Terms of Service has failed.
   */
  setTermsOfServiceLoadError() {
    // Disable the accept button, hide the iframe, show warning icon and retry
    // button.
    this.setUIStep(UIState.ERROR);

    this.acceptButtonDisabled_ = true;
    this.backButtonDisabled_ = false;
    this.retryButtonDisabled_ = false;
  },

  /**
   * Displays the given |termsOfService| and enables the accept button.
   * @param {string} termsOfService The terms of service, as plain text.
   */
  setTermsOfService(termsOfService) {
    this.$.termsOfServiceFrame.src =
        'data:text/html;charset=utf-8,' +
        encodeURIComponent(
            '<style>' +
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
            '<body><div id="tosContainer">' + termsOfService + '</div>' +
            '</body>');

    // Mark the loading as complete.
    this.acceptButtonDisabled_ = false;
    this.setUIStep(UIState.LOADED);
  },
});
})();
