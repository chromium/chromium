// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An UI component to let user init online re-auth flow on
 * the lock screen.
 */

import 'chrome://resources/ash/common/cr.m.js';
import 'chrome://resources/ash/common/event_target.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './components/buttons/oobe_text_button.js';
import './components/oobe_icons.html.js';
import './components/oobe_illo_icons.html.js';
import './gaia_action_buttons/gaia_action_buttons.js';
import '//resources/ash/common/cr_elements/policy/cr_tooltip_icon.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {AuthCompletedCredentials, AuthCompletedEvent, AuthDomainChangeEvent, Authenticator, AuthFlow, AuthFlowChangeEvent, AuthMode, AuthParams, LoadAbortEvent, SUPPORTED_PARAMS} from '//lock-reauth/gaia_auth_host/authenticator.js';
import {CrInputElement} from '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './lock_screen_reauth.html.js';

const clearDataType: chrome.webviewTag.ClearDataTypeSet = {
  appcache: true,
  cache: true,
  cookies: true,
};

interface LockReauthParams {
  webviewPartitionName: string;
  showVerificationNotice: boolean;
}

const LockReauthElementBase = I18nMixin(PolymerElement);

interface LockReauthElement {
  $: {
    confirmPasswordInput: CrInputElement,
    oldPasswordInput: CrInputElement,
    passwordInput: CrInputElement,
  };
}

class LockReauthElement extends LockReauthElementBase {
  static get is() {
    return 'lock-reauth';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * User non-canonicalized email for display
       */
      email: {
        type: String,
        value: '',
      },

      /**
       * Auth Domain property of the authenticator. Updated via events.
       */
      authDomain: {
        type: String,
        value: '',
      },

      /**
       * Whether the ‘verify user again’ screen is shown.
       */
      isErrorDisplayed: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the webview for online sign-in is shown.
       */
      isSigninFrameDisplayed: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the authenticator is currently showing SAML IdP page.
       */
      isSaml: {
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
      isConfirmPassword: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether no password is scraped or multiple passwords are scraped.
       */
      isManualInput: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the user's password has changed.
       */
      isPasswordChanged: {
        type: Boolean,
        value: false,
      },

      passwordConfirmAttempt: {
        type: Number,
        value: 0,
      },

      passwordChangeAttempt: {
        type: Number,
        value: 0,
      },
    };
  }

  email: string;
  authDomain: string;
  isButtonsEnabled: boolean;
  isErrorDisplayed: boolean;
  isSigninFrameDisplayed: boolean;
  isSaml: boolean;
  isDefaultSsoProvider: boolean;
  isConfirmPassword: boolean;
  isManualInput: boolean;
  isPasswordChanged: boolean;
  passwordConfirmAttempt: number;
  passwordChangeAttempt: number;

  /**
   * Saved authenticator load params.
   */
  private authenticatorParams: null|AuthParams = null;

  /**
   * The UI component that hosts IdP pages.
   */
  authenticator?: Authenticator;

  /**
   * Webview that view IdP page
   */
  private signinFrame?: chrome.webviewTag.WebView;


  override ready() {
    super.ready();
    this.signinFrame = this.getSigninFrame();
    const authenticator = this.authenticator =
        new Authenticator(this.signinFrame);

    const authenticatorEventListeners: Record<string, (e: any) => void> = {
      'authDomainChange': (e: AuthDomainChangeEvent) => {
        this.authDomain = e.detail.newValue;
      },
      'authCompleted': (e: AuthCompletedEvent) =>
          void this.onAuthCompletedMessage(e.detail),
      'loadAbort': (e: LoadAbortEvent) =>
          void this.onLoadAbortMessage(e.detail),
      'getDeviceId': (_: Event) => {
        sendWithPromise('getDeviceId')
            .then(deviceId => authenticator.getDeviceIdResponse(deviceId));
      },
      'authFlowChange': (e: AuthFlowChangeEvent) => {
        this.isSaml = e.detail.newValue === AuthFlow.SAML;
      },
    };

    for (const eventName in authenticatorEventListeners) {
      this.authenticator.addEventListener(
          eventName, authenticatorEventListeners[eventName].bind(this));
    }

    chrome.send('startOnlineAuth', /*force_reauth_gaia_page=*/[false]);
  }

  private resetState() {
    this.isErrorDisplayed = false;
    this.isSaml = false;
    this.isSigninFrameDisplayed = false;
    this.isConfirmPassword = false;
    this.isManualInput = false;
    this.isPasswordChanged = false;
    this.authDomain = '';
  }

  /**
   * Set the orientation which will be used in styling webui.
   * @param isHorizontal whether the orientation is horizontal or
   *  vertical.
   */
  setOrientation(isHorizontal: boolean) {
    if (isHorizontal) {
      document.documentElement.setAttribute('orientation', 'horizontal');
    } else {
      document.documentElement.setAttribute('orientation', 'vertical');
    }
  }

  /**
   * Set the width which will be used in styling webui.
   * @param width the width of the dialog.
   */
  setWidth(width: number) {
    document.documentElement.style.setProperty(
        '--lock-screen-reauth-dialog-width', width + 'px');
  }

  /**
   * Loads the authentication parameters.
   * @param data authenticator parameters bag.
   */
  loadAuthenticator(data: LockReauthParams&AuthParams) {
    assert(
        'webviewPartitionName' in data,
        'ERROR: missing webview partition name');
    assert(this.authenticator, 'ERROR: Authenticator not yet initialized');
    this.authenticator.setWebviewPartition(data.webviewPartitionName);

    const params: AuthParams = {} as AuthParams;
    SUPPORTED_PARAMS.forEach((name: string) => {
      if (data.hasOwnProperty(name)) {
        params[name] = data[name];
      }
    });

    params.enableGaiaActionButtons = data.enableGaiaActionButtons;
    this.authenticatorParams = params;
    this.email = data.email;
    this.isDefaultSsoProvider = !!data.doSamlRedirect;
    this.isSaml = this.isDefaultSsoProvider;
    this.doGaiaRedirect();

    chrome.send('authenticatorLoaded');
  }


  /**
   * This function is used when the wrong user is verified correctly
   * It reset authenticator state and display error message.
   */
  resetAuthenticator() {
    this.getSigninFrame().clearData({since: 0}, clearDataType, () => {
      this.authenticator!.resetStates();
      this.isButtonsEnabled = true;
      this.isErrorDisplayed = true;
    });
  }

  /**
   * Reloads the page.
   */
  reloadAuthenticator() {
    this.getSigninFrame().clearData({since: 0}, clearDataType, () => {
      this.authenticator!.resetStates();
    });
  }

  private getSigninFrame(): chrome.webviewTag.WebView {
    // Note: Can't use |this.$|, since it returns cached references to elements
    // originally present in DOM, while the signin-frame is dynamically
    // recreated (see Authenticator.setWebviewPartition()).
    const signinFrame = this.shadowRoot!.getElementById('signin-frame');
    assert(signinFrame, 'ERROR: signin-frame not found');
    return signinFrame as chrome.webviewTag.WebView;
  }

  private setFocusToWebview() {
    this.signinFrame!.focus();
  }

  onAuthCompletedMessage(credentials: AuthCompletedCredentials) {
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
   * @param data  Additional information about error event like:
   *     {number} error_code Error code such as net::ERR_INTERNET_DISCONNECTED.
   *     {string} src The URL that failed to load.
   */
  private onLoadAbortMessage(data: LoadAbortEvent['detail']) {
    chrome.send('webviewLoadAborted', [data.error_code]);
  }

  /**
   * Invoked when the user has successfully authenticated via SAML,
   * the Chrome Credentials Passing API was not used and the authenticator needs
   * the user to confirm the scraped password.
   * @param passwordCount The number of passwords that were scraped.
   */
  showSamlConfirmPassword(passwordCount: number) {
    this.resetState();
    /**
     * This statement override resetState calls.
     * Thus have to be AFTER resetState.
     */
    this.isConfirmPassword = true;
    this.isManualInput = (passwordCount === 0);
    if (this.passwordConfirmAttempt > 0) {
      this.$.passwordInput.value = '';
      this.$.passwordInput.invalid = true;
    }
    this.passwordConfirmAttempt++;
  }

  /**
   * Invoked when the user's password doesn't match his old password.
   */
  private passwordChanged() {
    this.resetState();
    this.isPasswordChanged = true;
    this.passwordChangeAttempt++;
    if (this.passwordChangeAttempt > 1) {
      this.$.oldPasswordInput.invalid = true;
    }
  }

  private onVerify() {
    assert(
        this.authenticatorParams,
        'ERROR: authenticator parameters not yet loaded');
    this.authenticator!.load(AuthMode.DEFAULT, this.authenticatorParams);
    this.resetState();
    /**
     * These statements override resetStates calls.
     * Thus have to be AFTER resetState.
     */
    this.isSigninFrameDisplayed = true;
  }

  private onConfirm() {
    if (!this.$.passwordInput.validate()) {
      return;
    }
    if (this.isManualInput) {
      // When using manual password entry, both passwords must match.
      if (!this.$.confirmPasswordInput.validate()) {
        return;
      }

      if (this.$.confirmPasswordInput.value !== this.$.passwordInput.value) {
        this.$.passwordInput.invalid = true;
        this.$.confirmPasswordInput.invalid = true;
        return;
      }
    }

    chrome.send('onPasswordTyped', [this.$.passwordInput.value]);
  }

  private onCloseClick() {
    chrome.send('dialogClose');
  }

  private onNext() {
    if (!this.$.oldPasswordInput.validate()) {
      this.$.oldPasswordInput.focusInput();
      return;
    }
    chrome.send('updateUserPassword', [this.$.oldPasswordInput.value]);
    this.$.oldPasswordInput.value = '';
  }

  private doGaiaRedirect() {
    assert(
        this.authenticatorParams,
        'ERROR: authenticator parameters not yet loaded');
    this.authenticator!.load(AuthMode.DEFAULT, this.authenticatorParams);
    this.resetState();
    /**
     * These statements override resetStates calls.
     * Thus have to be AFTER resetState.
     */
    this.isSigninFrameDisplayed = true;
  }

  private passwordPlaceholder(_locale: string, isManualInput: boolean) {
    return this.i18n(
        isManualInput ? 'manualPasswordInputLabel' : 'confirmPasswordLabel');
  }

  private passwordErrorText(_locale: string, isManualInput: boolean) {
    return this.i18n(
        isManualInput ? 'manualPasswordMismatch' :
                        'passwordChangedIncorrectOldPassword');
  }

  /**
   * Invoked when "Enter Google Account info" button is pressed on SAML screen.
   * Starts the reauth flow of GAIA.
   */
  private onChangeSigninProviderClicked() {
    this.resetState();
    chrome.send('startOnlineAuth', /*force_reauth_gaia_page=*/[true]);
  }

  private policyProvidedTrustedAnchorsUsed() {
    return loadTimeData.getBoolean('policyProvidedCaCertsPresent');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lock-reauth': LockReauthElement;
  }
}

customElements.define(LockReauthElement.is, LockReauthElement);
