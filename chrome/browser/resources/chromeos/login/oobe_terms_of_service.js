// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



// Enum that describes the current state of the Terms Of Service screen
var TermsOfServiceScreenState = {LOADING: 0, LOADED: 1, ERROR: 2};

/**
 * @fileoverview Polymer element for displaying material design Terms Of Service
 * screen.
 */
Polymer({
  is: 'terms-of-service',

  behaviors: [I18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  properties: {

    // Whether the back button is disabled.
    backButtonDisabled_: {type: Boolean, value: false},

    // Whether the accept button is disabled.
    acceptButtonDisabled_: {type: Boolean, value: true},

    // The domain that the terms of service belongs to.
    tosDomain_: {type: String, value: ''},

    // The current state of the screen.
    uiState: {type: Number, value: 0 /* TermsOfServiceScreenState.LOADING */},
  },

  // Whether the screen is still loading.
  isLoading_: function(state) {
    return state == TermsOfServiceScreenState.LOADING;
  },

  // Whether the screen has finished loading.
  isLoaded_: function(state) {
    return state == TermsOfServiceScreenState.LOADED;
  },

  // Whether the screen is in an error state.
  isInErrorState_: function(state) {
    return state == TermsOfServiceScreenState.ERROR;
  },

  EXTERNAL_API: [
    'setDomain',
    'setTermsOfServiceLoadError',
    'setTermsOfService',
  ],

  /** @override */
  ready: function() {
    this.initializeLoginScreen('TermsOfServiceScreen', {
      resetAllowed: true,
      enableDebuggingAllowed: true,
    });
  },


  focus: function() {
    this.$.termsOfServiceDialog.show();
  },

  /** Called when dialog is shown */
  onBeforeShow: function() {
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });
  },

  /**
   * This is called when strings are updated.
   * @override
   */
  updateLocalizedContent: function(event) {
    this.i18nUpdateLocale();
  },

  /**
   * The 'on-tap' event handler for the 'Accept' button.
   * @private
   */
  onTermsOfServiceAccepted_: function() {
    // Ignore on-tap events when disabled.
    // TODO: Polymer Migration - Remove this when the migration is finished.
    // See: https://github.com/Polymer/polymer/issues/4685
    if (this.acceptButtonDisabled_)
      return;

    this.backButtonDisabled_ = true;
    this.acceptButtonDisabled_ = true;
    chrome.send('termsOfServiceAccept');
  },

  /**
   * The 'on-tap' event handler for the 'Back' button.
   * @private
   */
  onTosBackButtonPressed_: function() {
    // Ignore on-tap events when disabled.
    // TODO: Polymer Migration - Remove this when the migration is finished.
    // See: https://github.com/Polymer/polymer/issues/4685
    if (this.backButtonDisabled_)
      return;

    this.backButtonDisabled_ = true;
    this.acceptButtonDisabled_ = true;
    chrome.send('termsOfServiceBack');
  },

  /**
   * Updates headings on the screen to indicate that the Terms of Service
   * being shown belong to |domain|.
   * @param {string} domain The domain whose Terms of Service are being shown.
   */
  setDomain: function(domain) {
    this.tosDomain_ = domain;
  },

  /**
   * Displays an error message on the Terms of Service screen. Called when the
   * download of the Terms of Service has failed.
   */
  setTermsOfServiceLoadError: function() {
    // Disable the accept button, hide the iframe, show warning icon.
    this.uiState = TermsOfServiceScreenState.ERROR;

    this.acceptButtonDisabled_ = true;
    this.backButtonDisabled_ = false;
  },

  /**
   * Displays the given |termsOfService| and enables the accept button.
   * @param {string} termsOfService The terms of service, as plain text.
   */
  setTermsOfService: function(termsOfService) {
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
            '  border: 0.5px solid rgb(224, 224, 224);' +
            '  border-radius: 4px;' +
            '  box-shadow: inset 0 0 4px 4px rgba(224, 224, 224, .5);' +
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
    this.uiState = TermsOfServiceScreenState.LOADED;
  },

});
