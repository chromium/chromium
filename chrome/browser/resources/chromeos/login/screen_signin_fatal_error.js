// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'signin-fatal-error-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  properties: {
    /**
     * Subtitle that will be shown to the user describing the error
     * @private
     */
    errorSubtitle_: {
      type: String,
      computed: 'computeSubtitle_(locale, errorState_, params_)'
    },

    /**
     * Error state from the screen
     * @private
     */
    errorState_: {
      type: Number,
      value: 0,
    },

    /**
     * Additional information that will be used when creating the subtitle.
     * @private
     */
    params_: {
      type: Object,
      value: {},
    },
  },

  ready() {
    this.initializeLoginScreen('SignInFatalErrorScreen', {
      resetAllowed: true,
    });
  },

  onClick_() {
    this.userActed('screen-dismissed');
  },

  // Invoked just before being shown. Contains all the data for the screen.
  onBeforeShow(data) {
    this.errorState_ = data && 'errorState' in data && data.errorState;
    this.params_ = data;
  },

  /**
   * Generates the key for the button that is shown to the
   * user based on the error
   * @param {number} error_state
   * @private
   */
  computeButtonKey_(error_state) {
    if (this.errorState_ == OobeTypes.FatalErrorCode.INSECURE_CONTENT_BLOCKED) {
      return 'fatalErrorDoneButton';
    }

    return 'fatalErrorTryAgainButton';
  },

  /**
   * Generates the subtitle that is shown to the
   * user based on the error
   * @param {string} locale
   * @param {number} error_state
   * @param {string} params
   * @private
   */
  computeSubtitle_(locale, error_state, params) {
    switch (this.errorState_) {
      case OobeTypes.FatalErrorCode.SCRAPED_PASSWORD_VERIFICATION_FAILURE:
        return this.i18n('fatalErrorMessageVerificationFailed');
      case OobeTypes.FatalErrorCode.MISSING_GAIA_INFO:
        return this.i18n('fatalErrorMessageNoAccountDetails');
      case OobeTypes.FatalErrorCode.INSECURE_CONTENT_BLOCKED:
        return this.i18n(
            'fatalErrorMessageInsecureURL',
            'url' in this.params_ && this.params_.url);
      case OobeTypes.FatalErrorCode.UNKNOWN:
        return '';
    }
  },

  get defaultControl() {
    return this.$.actionButton;
  },

});