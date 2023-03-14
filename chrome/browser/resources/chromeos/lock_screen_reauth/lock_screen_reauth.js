// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An UI component to let user init online re-auth flow on
 * the lock screen.
 */

import 'chrome://resources/ash/common/cr.m.js';
import 'chrome://resources/ash/common/event_target.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './components/buttons/oobe_text_button.js';
import './components/oobe_icons.html.js';
import '//resources/cr_elements/policy/cr_tooltip_icon.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Authenticator, AuthMode, AuthParams, SUPPORTED_PARAMS} from '../../gaia_auth_host/authenticator.js';

const clearDataType = {
  appcache: true,
  cache: true,
  cookies: true,
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const LockReauthBase = mixinBehaviors([I18nBehavior], PolymerElement);

/**
 * @polymer
 */
class LockReauth extends LockReauthBase {
  static get is() {
    return 'lock-reauth';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * User non-canonicalized email for display
       */
      email_: {
        type: String,
        value: '',
      },

      /**
       * Auth Domain property of the authenticator. Updated via events.
       */
      authDomain_: {
        type: String,
        value: '',
      },

      /**
       * Whether the ‘verify user’ screen is shown.
       */
      isVerifyUser_: {
        type: Boolean,
        value: true,
      },

      /**
       * Whether the ‘verify user again’ screen is shown.
       */
      isErrorDisplayed_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether user is authenticating on SAML page.
       */
      isSamlPage_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether default SAML IdP is shown.
       */
      isDefaultSsoProvider: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether there is a failure to scrape the user's password.
       */
      isConfirmPassword_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether no password is scraped or multiple passwords are scraped.
       */
      isManualInput_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the user's password has changed.
       */
      isPasswordChanged_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether to show Saml Notice Message.
       */
      showSamlNoticeMessage_: {
        type: Boolean,
        value: false,
      },

      passwordConfirmAttempt_: {
        type: Number,
        value: 0,
      },

      passwordChangeAttempt_: {
        type: Number,
        value: 0,
      },
    };
  }

  constructor() {
    super();

    /**
     * Saved authenticator load params.
     * @type {?AuthParams}
     * @private
     */
    this.authenticatorParams_ = null;

    /**
     * The UI component that hosts IdP pages.
     * @type {!Authenticator|undefined}
     */
    this.authenticator_ = undefined;

    /**
     * Webview that view IdP page
     * @type {!WebView|undefined}
     * @private
     */
    this.signinFrame_ = undefined;
  }

  /** @override */
  ready() {
    super.ready();
    this.signinFrame_ = this.getSigninFrame_();
    this.authenticator_ = new Authenticator(this.signinFrame_);
    this.authenticator_.addEventListener('authDomainChange', (e) => {
      this.authDomain_ = e.detail.newValue;
    });
    this.authenticator_.addEventListener(
        'authCompleted', (e) => void this.onAuthCompletedMessage_(e));
    this.authenticator_.addEventListener(
        'loadAbort', (e) => void this.onLoadAbortMessage_(e.detail));
    chrome.send('initialize');
  }

  /** @private */
  resetState_() {
    this.isVerifyUser_ = false;
    this.isErrorDisplayed_ = false;
    this.isSamlPage_ = false;
    this.isConfirmPassword_ = false;
    this.isManualInput_ = false;
    this.isPasswordChanged_ = false;
    this.showSamlNoticeMessage_ = false;
    this.authDomain_ = '';
  }

  /**
   * Set the orientation which will be used in styling webui.
   * @param {!Object} is_horizontal whether the orientation is horizontal or
   *  vertical.
   */
  setOrientation(is_horizontal) {
    if (is_horizontal) {
      document.documentElement.setAttribute('orientation', 'horizontal');
    } else {
      document.documentElement.setAttribute('orientation', 'vertical');
    }
  }

  /**
   * Set the width which will be used in styling webui.
   * @param {!Object} width the width of the dialog.
   */
  setWidth(width) {
    document.documentElement.style.setProperty(
        '--lock-screen-reauth-dialog-width', width + 'px');
  }

  /**
   * Loads the authentication parameter into the iframe.
   * @param {!Object} data authenticator parameters bag.
   */
  loadAuthenticator(data) {
    assert(
        'webviewPartitionName' in data,
        'ERROR: missing webview partition name');
    this.authenticator_.setWebviewPartition(data.webviewPartitionName);
    const params = {};
    SUPPORTED_PARAMS.forEach(name => {
      if (data.hasOwnProperty(name)) {
        params[name] = data[name];
      }
    });

    this.authenticatorParams_ = /** @type {AuthParams} */ (params);
    this.email_ = data.email;
    this.isDefaultSsoProvider = data.doSamlRedirect;
    if (!data['doSamlRedirect']) {
      this.doGaiaRedirect_();
    }
    chrome.send('authenticatorLoaded');
  }


  /**
   * This function is used when the wrong user is verified correctly
   * It reset authenticator state and display error message.
   */
  resetAuthenticator() {
    this.signinFrame_.clearData({since: 0}, clearDataType, () => {
      this.authenticator_.resetStates();
      this.isButtonsEnabled_ = true;
      this.isErrorDisplayed_ = true;
    });
  }

  /**
   * Reloads the page.
   */
  reloadAuthenticator() {
    this.signinFrame_.clearData({since: 0}, clearDataType, () => {
      this.authenticator_.resetStates();
    });
  }

  /**
   * @return {!WebView}
   * @private
   */
  getSigninFrame_() {
    // Note: Can't use |this.$|, since it returns cached references to elements
    // originally present in DOM, while the signin-frame is dynamically
    // recreated (see Authenticator.setWebviewPartition()).
    const signinFrame = this.shadowRoot.getElementById('signin-frame');
    assert(signinFrame);
    return /** @type {!WebView} */ (signinFrame);
  }

  onAuthCompletedMessage_(e) {
    const credentials = e.detail;
    chrome.send('completeAuthentication', [
      credentials.gaiaId,
      credentials.email,
      credentials.password,
      credentials.scrapedSAMLPasswords,
      credentials.usingSAML,
      credentials.services,
      credentials.passwordAttributes,
    ]);
  }

  /**
   * Invoked when onLoadAbort message received.
   * @param {!Object} data  Additional information about error event like:
   *     {number} error_code Error code such as net::ERR_INTERNET_DISCONNECTED.
   *     {string} src The URL that failed to load.
   * @private
   */
  onLoadAbortMessage_(data) {
    chrome.send('webviewLoadAborted', [data.error_code]);
  }

  /**
   * Invoked when the user has successfully authenticated via SAML,
   * the Chrome Credentials Passing API was not used and the authenticator needs
   * the user to confirm the scraped password.
   * @param {number} passwordCount The number of passwords that were scraped.
   */
  showSamlConfirmPassword(passwordCount) {
    this.resetState_();
    /**
     * This statement override resetState_ calls.
     * Thus have to be AFTER resetState_.
     */
    this.isConfirmPassword_ = true;
    this.isManualInput_ = (passwordCount === 0);
    if (this.passwordConfirmAttempt_ > 0) {
      this.$.passwordInput.value = '';
      this.$.passwordInput.invalid = true;
    }
    this.passwordConfirmAttempt_++;
  }

  /**
   * Invoked when the user's password doesn't match his old password.
   * @private
   */
  passwordChanged() {
    this.resetState_();
    this.isPasswordChanged_ = true;
    this.passwordChangeAttempt_++;
    if (this.passwordChangeAttempt_ > 1) {
      this.$.oldPasswordInput.invalid = true;
    }
  }

  /** @private */
  onVerify_() {
    this.authenticator_.load(
        AuthMode.DEFAULT,
        /** @type {AuthParams} */ (this.authenticatorParams_));
    this.resetState_();
    /**
     * These statements override resetStates_ calls.
     * Thus have to be AFTER resetState_.
     */
    this.isSamlPage_ = true;
    this.showSamlNoticeMessage_ = true;
  }

  /** @private */
  onConfirm_() {
    if (!this.$.passwordInput.validate()) {
      return;
    }
    if (this.isManualInput_) {
      // When using manual password entry, both passwords must match.
      const confirmPasswordInput =
          this.shadowRoot.querySelector('#confirmPasswordInput');
      if (!confirmPasswordInput.validate()) {
        return;
      }

      if (confirmPasswordInput.value != this.$.passwordInput.value) {
        this.$.passwordInput.invalid = true;
        confirmPasswordInput.invalid = true;
        return;
      }
    }

    chrome.send('onPasswordTyped', [this.$.passwordInput.value]);
  }

  /** @private */
  onCloseTap_() {
    chrome.send('dialogClose');
  }

  /** @private */
  onNext_() {
    if (!this.$.oldPasswordInput.validate()) {
      this.$.oldPasswordInput.focusInput();
      return;
    }
    chrome.send('updateUserPassword', [this.$.oldPasswordInput.value]);
    this.$.oldPasswordInput.value = '';
  }

  /** @private */
  doGaiaRedirect_() {
    this.authenticator_.load(
        AuthMode.DEFAULT,
        /** @type {AuthParams} */ (this.authenticatorParams_));
    this.resetState_();
    /**
     * These statements override resetStates_ calls.
     * Thus have to be AFTER resetState_.
     */
    this.isSamlPage_ = true;
  }

  /** @private */
  passwordPlaceholder_(locale, isManualInput_) {
    return this.i18n(
        isManualInput_ ? 'manualPasswordInputLabel' : 'confirmPasswordLabel');
  }

  /** @private */
  passwordErrorText_(locale, isManualInput_) {
    return this.i18n(
        isManualInput_ ? 'manualPasswordMismatch' :
                         'passwordChangedIncorrectOldPassword');
  }

  /**
   * Invoked when "Enter Google Account info" button is pressed on SAML screen.
   * @private
   */
  onChangeSigninProviderClicked_() {
    this.authenticatorParams_.doSamlRedirect = false;
    this.authenticatorParams_.enableGaiaActionButtons = true;
    this.isDefaultSsoProvider = false;
    this.authenticator_.load(
        AuthMode.DEFAULT,
        /** @type {AuthParams} */ (this.authenticatorParams_));
  }

  /** @private */
  policyProvidedTrustedAnchorsUsed_() {
    return loadTimeData.getBoolean('policyProvidedCaCertsPresent');
  }
}

customElements.define(LockReauth.is, LockReauth);
