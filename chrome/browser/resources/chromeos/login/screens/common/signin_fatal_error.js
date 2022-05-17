// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for signin fatal error.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const SigninFatalErrorBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @typedef {{
 *   actionButton:  OobeTextButton,
 * }}
 */
SigninFatalErrorBase.$;

/**
 * @polymer
 */
class SigninFatalScreen extends SigninFatalErrorBase {
  static get is() {
    return 'signin-fatal-error-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
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
      },

      /**
       * Additional information that will be used when creating the subtitle.
       * @private
       */
      params_: {
        type: Object,
      },

      keyboardHint_: {
        type: String,
      },

      details_: {
        type: String,
      },

      helpLinkText_: {
        type: String,
      },
    };
  }

  constructor() {
    super();
    this.errorState_ = 0;
    this.params_ = {};
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('SignInFatalErrorScreen');
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.BLOCKING;
  }

  /**
   * Returns the control which should receive initial focus.
   */
  get defaultControl() {
    return this.$.actionButton;
  }

  /**
   * Invoked just before being shown. Contains all the data for the screen.
   * @suppress {missingProperties} params_ fields.
   */
  onBeforeShow(data) {
    this.errorState_ = data && 'errorState' in data && data.errorState;
    this.params_ = data;
    this.keyboardHint_ = this.params_.keyboardHint;
    this.details_ = this.params_.details;
    this.helpLinkText_ = this.params_.helpLinkText;
  }

  onClick_() {
    this.userActed('screen-dismissed');
  }

  /**
   * Generates the key for the button that is shown to the
   * user based on the error
   * @param {number} error_state
   * @private
   * @suppress {missingProperties} OobeTypes
   */
  computeButtonKey_(error_state) {
    if (this.errorState_ == OobeTypes.FatalErrorCode.INSECURE_CONTENT_BLOCKED) {
      return 'fatalErrorDoneButton';
    }

    return 'fatalErrorTryAgainButton';
  }

  /**
   * Generates the subtitle that is shown to the
   * user based on the error
   * @param {string} locale
   * @param {number} error_state
   * @param {string} params
   * @private
   * @suppress {missingProperties} errorText in this.params_
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
      case OobeTypes.FatalErrorCode.CUSTOM:
        return this.params_.errorText;
      case OobeTypes.FatalErrorCode.UNKNOWN:
        return '';
    }
  }

  onHelpLinkClicked_() {
    this.userActed('learn-more');
  }
}

customElements.define(SigninFatalScreen.is, SigninFatalScreen);
