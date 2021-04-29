// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe signin screen implementation.
 */

'use strict';

(function() {

// GAIA animation guard timer. Started when GAIA page is loaded (Authenticator
// 'ready' event) and is intended to guard against edge cases when 'showView'
// message is not generated/received.
const GAIA_ANIMATION_GUARD_MILLISEC = 300;

// Maximum Gaia loading time in seconds.
const MAX_GAIA_LOADING_TIME_SEC = 60;

// The help topic regarding user not being in the allowlist.
const HELP_CANT_ACCESS_ACCOUNT = 188036;

// Amount of time allowed for video based SAML logins, to prevent a site from
// keeping the camera on indefinitely.  This is a hard deadline and it will
// not be extended by user activity.
const VIDEO_LOGIN_TIMEOUT = 90 * 1000;

// Horizontal padding for the error bubble.
const BUBBLE_HORIZONTAL_PADDING = 65;

// Vertical padding for the error bubble.
const BUBBLE_VERTICAL_PADDING = -213;

/**
 * The authentication mode for the screen.
 * @enum {number}
 */
const AuthMode = {
  DEFAULT: 0,            // Default GAIA login flow.
  SAML_INTERSTITIAL: 1,  // Interstitial page before SAML redirection.
};

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const DialogMode = {
  GAIA: 'online-gaia',
  GAIA_LOADING: 'gaia-loading',
  LOADING: 'loading',
  PIN_DIALOG: 'pin',
  GAIA_ALLOWLIST_ERROR: 'allowlist-error',
  SAML_INTERSTITIAL: 'saml-interstitial',
};

/**
 * Steps that could be the first one in the flow.
 */
const POSSIBLE_FIRST_SIGNIN_STEPS =
    [DialogMode.GAIA, DialogMode.GAIA_LOADING, DialogMode.SAML_INTERSTITIAL];

Polymer({
  is: 'gaia-signin-element',

  behaviors: [
    OobeI18nBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  EXTERNAL_API: [
    'loadAuthExtension',
    'doReload',
    'showAllowlistCheckFailedError',
    'showPinDialog',
    'closePinDialog',
    'clickPrimaryButtonForTesting',
  ],

  properties: {

    /**
     * Current mode of this screen.
     * @private
     */
    screenMode_: {
      type: Number,
      value: AuthMode.DEFAULT,
    },

    /**
     * Whether the screen contents are currently being loaded.
     * @private
     */
    loadingFrameContents_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the loading UI is shown.
     * @private
     */
    isLoadingUiShown_: {
      type: Boolean,
      computed: 'computeIsLoadingUiShown_(loadingFrameContents_, ' +
          'isAllowlistErrorShown_, authCompleted_)',
    },

    /**
     * Whether the loading allowlist error UI is shown.
     * @private
     */
    isAllowlistErrorShown_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the navigation controls are enabled.
     * @private
     */
    navigationEnabled_: {
      type: Boolean,
      value: true,
    },

    /**
     * Whether the authenticator is currently in the |SAML| AuthFlow.
     * @private
     */
    isSaml_: {
      type: Boolean,
      value: false,
      observer: 'onSamlChanged_',
    },

    /**
     * Whether the authenticator is or has been in the |SAML| AuthFlow during
     * the current authentication attempt.
     * @private
     */
    usedSaml_: {
      type: Boolean,
      value: false,
    },

    /**
     * Management domain displayed on SAML interstitial page.
     * @private
     */
    samlInterstitialDomain_: {
      type: String,
      value: null,
    },

    /**
     * Contains the security token PIN dialog parameters object when the dialog
     * is shown. Is null when no PIN dialog is shown.
     * @type {OobeTypes.SecurityTokenPinDialogParameter}
     * @private
     */
    pinDialogParameters_: {
      type: Object,
      value: null,
      observer: 'onPinDialogParametersChanged_',
    },

    /**
     * Whether the SAML 3rd-party page is visible.
     * @private
     */
    isSamlSsoVisible_: {
      type: Boolean,
      computed: 'computeSamlSsoVisible_(isSaml_, pinDialogParameters_)',
    },

    /**
     * Bound to gaia-dialog::videoEnabled.
     * @private
     */
    videoEnabled_: {
      type: Boolean,
      observer: 'onVideoEnabledChange_',
    },

    /**
     * Bound to gaia-dialog::authFlow.
     * @private
     */
    authFlow: {
      type: Number,
      observer: 'onAuthFlowChange_',
    },

    /**
     * @private
     */
    navigationButtonsHidden_: {
      type: Boolean,
      value: false,
    },

    /**
     * Bound to gaia-dialog::canGoBack.
     * @private
     */
    canGaiaGoBack_: {
      type: Boolean,
    },

    /*
     * Updates whether the Guest and Apps button is allowed to be shown.
     * (Note that the C++ side contains additional logic that decides whether
     * the Guest button should be shown.)
     * @private
     */
    isFirstSigninStep_: {
      type: Boolean,
      computed: 'isFirstSigninStep(uiStep, canGaiaGoBack_, isSaml_)',
      observer: 'onIsFirstSigninStepChanged'
    },

    /*
     * Whether the screen is shown.
     * @private
     */
    isShown_: {
      type: Boolean,
      value: false,
    },
  },

  observers: [
    'refreshDialogStep_(isShown_, screenMode_, pinDialogParameters_,' +
        'isLoadingUiShown_, isAllowlistErrorShown_)',
  ],

  /**
   * Saved authenticator load params.
   * @type {?string}
   * @private
   */
  authenticatorParams_: null,

  /**
   * Email of the user, which is logging in using offline mode.
   * @type {string}
   * @private
   */
  email_: '',

  /**
   * Timer id of pending load.
   * @type {number}
   * @private
   */
  loadingTimer_: undefined,

  /**
   * Timer id of a guard timer that is fired in case 'showView' message is not
   * received from GAIA.
   * @type {number}
   * @private
   */
  loadAnimationGuardTimer_: undefined,

  /**
   * Timer id of the video login timer.
   * @type {number}
   * @private
   */
  videoTimer_: undefined,

  /**
   * Whether we've processed 'showView' message - either from GAIA or from
   * guard timer.
   * @type {boolean}
   * @private
   */
  showViewProcessed_: false,

  /**
   * Whether we've processed 'authCompleted' message.
   * @type {boolean}
   * @private
   */
  authCompleted_: false,

  /**
   * SAML password confirmation attempt count.
   * @type {number}
   * @private
   */
  samlPasswordConfirmAttempt_: 0,

  /**
   * Whether the result was reported to the handler for the most recent PIN
   * dialog.
   * @type {boolean}
   * @private
   */
  pinDialogResultReported_: false,

  defaultUIStep() {
    return DialogMode.GAIA;
  },

  UI_STEPS: DialogMode,

  get authenticator_() {
    return this.$['signin-frame-dialog'].getAuthenticator();
  },

  /** @override */
  ready() {
    this.authenticator_.addEventListener(
        'authCompleted', this.onAuthCompletedMessage_.bind(this));

    this.authenticator_.confirmPasswordCallback =
        this.onAuthConfirmPassword_.bind(this);
    this.authenticator_.onePasswordCallback =
        this.onAuthOnePassword_.bind(this);
    this.authenticator_.noPasswordCallback = this.onAuthNoPassword_.bind(this);
    this.authenticator_.insecureContentBlockedCallback =
        this.onInsecureContentBlocked_.bind(this);
    this.authenticator_.missingGaiaInfoCallback =
        this.missingGaiaInfo_.bind(this);
    this.authenticator_.samlApiUsedCallback = this.samlApiUsed_.bind(this);
    this.authenticator_.recordSAMLProviderCallback =
        this.recordSAMLProvider_.bind(this);
    this.authenticator_.getIsSamlUserPasswordlessCallback =
        this.getIsSamlUserPasswordless_.bind(this);

    this.$['gaia-allowlist-error'].addEventListener('buttonclick', function() {
      this.showAllowlistCheckFailedError(false);
    }.bind(this));

    this.$['gaia-allowlist-error'].addEventListener('linkclick', function() {
      chrome.send('launchHelpApp', [HELP_CANT_ACCESS_ACCOUNT]);
    });

    this.initializeLoginScreen('GaiaSigninScreen', {
      resetAllowed: true,
    });
  },

  /**
   * Whether the dialog could be closed.
   * This is being checked in cancel() when user clicks on signin-back-button
   * (normal gaia flow) or the "Back" button in other authentication frames.
   * @return {boolean}
   * @private
   */
  isClosable_() {
    return Oobe.getInstance().hasUserPods;
  },

  /**
   * Updates whether the Guest and Apps button is allowed to be shown. (Note
   * that the C++ side contains additional logic that decides whether the
   * Guest button should be shown.)
   * @private
   */
  isFirstSigninStep(uiStep, canGaiaGoBack, isSaml) {
    return !this.isClosable_() &&
        POSSIBLE_FIRST_SIGNIN_STEPS.includes(uiStep) && !canGaiaGoBack &&
        !isSaml;
  },

  onIsFirstSigninStepChanged(isFirstSigninStep) {
    chrome.send('setIsFirstSigninStep', [isFirstSigninStep]);
  },

  /**
   * Handles clicks on "Back" button.
   * @private
   */
  onBackButtonCancel_() {
    if (!this.authCompleted_) {
      this.cancel(true /* isBackClicked */);
    }
  },

  onInterstitialBackButtonClicked_() {
    this.cancel(true /* isBackClicked */);
  },

  /**
   * Loads the authenticator and updates the UI to reflect the loading state.
   * @param {boolean} doSamlRedirect If the authenticator should do
   *     authentication by automatic redirection to the SAML-based enrollment
   *     enterprise domain IdP.
   * @private
   */
  loadAuthenticator_(doSamlRedirect) {
    this.loadingFrameContents_ = true;
    this.startLoadingTimer_();

    this.authenticatorParams_.doSamlRedirect = doSamlRedirect;
    this.authenticator_.load(
        cr.login.Authenticator.AuthMode.DEFAULT, this.authenticatorParams_);
  },

  /**
   * Whether the SAML 3rd-party page is visible.
   * @param {boolean} isSaml
   * @param {OobeTypes.SecurityTokenPinDialogParameters} pinDialogParameters
   * @return {boolean}
   * @private
   */
  computeSamlSsoVisible_(isSaml, pinDialogParameters) {
    return isSaml && !pinDialogParameters;
  },

  /**
   * Handler for Gaia loading timeout.
   * @private
   */
  onLoadingTimeOut_() {
    if (Oobe.getInstance().currentScreen.id != 'gaia-signin')
      return;
    this.loadingTimer_ = undefined;
    chrome.send('showLoadingTimeoutError');
  },

  /**
   * Clears loading timer.
   * @private
   */
  clearLoadingTimer_() {
    if (this.loadingTimer_) {
      clearTimeout(this.loadingTimer_);
      this.loadingTimer_ = undefined;
    }
  },

  /**
   * Sets up loading timer.
   * @private
   */
  startLoadingTimer_() {
    this.clearLoadingTimer_();
    this.loadingTimer_ = setTimeout(
        this.onLoadingTimeOut_.bind(this), MAX_GAIA_LOADING_TIME_SEC * 1000);
  },

  /**
   * Handler for GAIA animation guard timer.
   * @private
   */
  onLoadAnimationGuardTimer_() {
    this.loadAnimationGuardTimer_ = undefined;
    this.onShowView_();
  },

  /**
   * Clears GAIA animation guard timer.
   * @private
   */
  clearLoadAnimationGuardTimer_() {
    if (this.loadAnimationGuardTimer_) {
      clearTimeout(this.loadAnimationGuardTimer_);
      this.loadAnimationGuardTimer_ = undefined;
    }
  },

  /**
   * Sets up GAIA animation guard timer.
   * @private
   */
  startLoadAnimationGuardTimer_() {
    this.clearLoadAnimationGuardTimer_();
    this.loadAnimationGuardTimer_ = setTimeout(
        this.onLoadAnimationGuardTimer_.bind(this),
        GAIA_ANIMATION_GUARD_MILLISEC);
  },

  getOobeUIInitialState() {
    return OOBE_UI_STATE.GAIA_SIGNIN;
  },

  /**
   * Event handler that is invoked just before the frame is shown.
   */
  onBeforeShow() {
    chrome.send('loginUIStateChanged', ['gaia-signin', true]);

    // Ensure that GAIA signin (or loading UI) is actually visible.
    window.requestAnimationFrame(function() {
      chrome.send('loginVisible', ['gaia-loading']);
    });

    // Re-enable navigation in case it was disabled before refresh.
    this.navigationEnabled_ = true;

    this.isShown_ = true;

    cr.ui.login.invokePolymerMethod(this.$.pinDialog, 'onBeforeShow');
  },

  /**
   * @return {!Element}
   * @private
   */
  getSigninFrame_() {
    return this.$['signin-frame-dialog'].getFrame();
  },

  /** @private */
  getActiveFrame_() {
    switch (this.screenMode_) {
      case AuthMode.DEFAULT:
        return this.getSigninFrame_();
      case AuthMode.SAML_INTERSTITIAL:
        return this.$['saml-interstitial'];
    }
  },

  /** @private */
  focusActiveFrame_() {
    let activeFrame = this.getActiveFrame_();
    Polymer.RenderStatus.afterNextRender(this, () => activeFrame.focus());
  },

  /** Event handler that is invoked after the screen is shown. */
  onAfterShow() {
    if (!this.isLoadingUiShown_)
      this.focusActiveFrame_();
  },

  /**
   * Event handler that is invoked just before the screen is hidden.
   */
  onBeforeHide() {
    chrome.send('loginUIStateChanged', ['gaia-signin', false]);
    this.isShown_ = false;
  },

  /**
   * Loads the authentication extension into the iframe.
   * @param {!Object} data Extension parameters bag.
   */
  loadAuthExtension(data) {
    // Redirect the webview to the blank page in order to stop the SAML IdP
    // page from working in a background (see crbug.com/613245).
    if (this.screenMode_ == AuthMode.DEFAULT &&
        data.screenMode != AuthMode.DEFAULT) {
      this.authenticator_.resetWebview();
    }

    this.authenticator_.setWebviewPartition(data.webviewPartitionName);

    this.screenMode_ = data.screenMode;
    this.authCompleted_ = false;
    this.navigationButtonsHidden_ = false;

    // Reset SAML
    this.isSaml_ = false;
    this.usedSaml_ = false;
    this.samlPasswordConfirmAttempt_ = 0;

    // Reset the PIN dialog, in case it's shown.
    this.closePinDialog();

    let params = {};
    for (let i in cr.login.Authenticator.SUPPORTED_PARAMS) {
      const name = cr.login.Authenticator.SUPPORTED_PARAMS[i];
      if (data[name])
        params[name] = data[name];
    }

    params.doSamlRedirect = (this.screenMode_ == AuthMode.SAML_INTERSTITIAL);
    params.menuEnterpriseEnrollment =
        !(data.enterpriseManagedDevice || data.hasDeviceOwner);
    params.isFirstUser = !(data.enterpriseManagedDevice || data.hasDeviceOwner);
    params.obfuscatedOwnerId = data.obfuscatedOwnerId;
    params.enableGaiaActionButtons = true;

    this.authenticatorParams_ = params;

    switch (this.screenMode_) {
      case AuthMode.DEFAULT:
        this.loadAuthenticator_(false /* doSamlRedirect */);
        break;
      case AuthMode.SAML_INTERSTITIAL:
        this.samlInterstitialDomain_ = data.enterpriseDisplayDomain;
        this.loadingFrameContents_ = false;
        break;
    }
    chrome.send('authExtensionLoaded');
  },

  /**
   * Whether the current auth flow is SAML.
   * @return {boolean}
   */
  isSamlForTesting() {
    return this.isSaml_;
  },

  /**
   * Clean up from a video-enabled SAML flow.
   * @private
   */
  clearVideoTimer_() {
    if (this.videoTimer_ !== undefined) {
      clearTimeout(this.videoTimer_);
      this.videoTimer_ = undefined;
    }
  },

  /**
   * @private
   */
  onVideoEnabledChange_() {
    if (this.videoEnabled_ && this.videoTimer_ === undefined) {
      this.videoTimer_ =
          setTimeout(this.cancel.bind(this), VIDEO_LOGIN_TIMEOUT);
    } else {
      this.clearVideoTimer_();
    }
  },

  /**
   * Invoked when the authFlow property is changed on the authenticator.
   * @private
   */
  onAuthFlowChange_() {
    this.isSaml_ = this.authFlow == cr.login.Authenticator.AuthFlow.SAML;
  },

  /**
   * Observer that is called when the |isSaml_| property gets changed.
   * @param {number} newValue
   * @param {number} oldValue
   * @private
   */
  onSamlChanged_(newValue, oldValue) {
    if (this.isSaml_)
      this.usedSaml_ = true;

    chrome.send('samlStateChanged', [this.isSaml_]);

    this.classList.toggle('saml', this.isSaml_);

    // Skip these updates in the initial observer run, which is happening during
    // the property initialization.
    if (oldValue !== undefined) {
      if (Oobe.getInstance().currentScreen.id == 'gaia-signin') {
        Oobe.getInstance().updateScreenSize(this);
      }
    }
  },

  /**
   * Invoked when the authenticator emits 'ready' event or when another
   * authentication frame is completely loaded.
   * @private
   */
  onAuthReady_() {
    this.showViewProcessed_ = false;
    this.startLoadAnimationGuardTimer_();
    this.clearLoadingTimer_();
    // Workaround to hide flashing scroll bar.
    this.async(function() {
      this.loadingFrameContents_ = false;
    }.bind(this), 100);
  },

  /**
   * @private
   */
  onStartEnrollment_() {
    this.userActed('startEnrollment');
  },

  /**
   * Invoked when the authenticator requests whether the specified user is a
   * user without a password (neither a manually entered one nor one provided
   * via Credentials Passing API).
   * @param {string} email
   * @param {string} gaiaId
   * @param {function(boolean)} callback
   * @private
   */
  getIsSamlUserPasswordless_(email, gaiaId, callback) {
    cr.sendWithPromise('getIsSamlUserPasswordless', email, gaiaId)
        .then(callback);
  },

  /**
   * Invoked when the authenticator emits 'showView' event or when corresponding
   * guard time fires.
   * @private
   */
  onShowView_() {
    if (this.showViewProcessed_)
      return;

    this.showViewProcessed_ = true;
    this.clearLoadAnimationGuardTimer_();
    this.onLoginUIVisible_();
  },

  /**
   * Called when UI is shown.
   * @private
   */
  onLoginUIVisible_() {
    // Show deferred error bubble.
    if (this.errorBubble_) {
      this.showErrorBubble(this.errorBubble_[0], this.errorBubble_[1]);
      this.errorBubble_ = undefined;
    }

    chrome.send('loginWebuiReady');
    chrome.send('loginVisible', ['gaia-signin']);
  },

  /**
   * Invoked when the user has successfully authenticated via SAML,
   * the Chrome Credentials Passing API was not used and the authenticator needs
   * the user to confirm the scraped password.
   * @param {string} email The authenticated user's e-mail.
   * @param {number} passwordCount The number of passwords that were scraped.
   * @private
   */
  onAuthConfirmPassword_(email, passwordCount) {
    if (this.samlPasswordConfirmAttempt_ == 0)
      chrome.send('scrapedPasswordCount', [passwordCount]);

    if (this.samlPasswordConfirmAttempt_ < 2) {
      login.ConfirmSamlPasswordScreen.show(
          email, false /* manual password entry */,
          this.samlPasswordConfirmAttempt_,
          this.onConfirmPasswordCollected_.bind(this));
    } else {
      chrome.send('scrapedPasswordVerificationFailed');
      this.showFatalAuthError_(
          OobeTypes.FatalErrorCode.SCRAPED_PASSWORD_VERIFICATION_FAILURE);
    }
  },

  /**
   * Invoked when the user has successfully authenticated via SAML,
   * the Chrome Credentials Passing API was not used and exactly one password
   * was scraped (so we didn't have to ask the user to confirm their password).
   * @private
   */
  onAuthOnePassword_() {
    chrome.send('scrapedPasswordCount', [1]);
  },

  /**
   * Invoked when the confirm password screen is dismissed.
   * @param {string} password The password entered at the confirm screen.
   * @private
   */
  onConfirmPasswordCollected_(password) {
    this.samlPasswordConfirmAttempt_++;
    this.authenticator_.verifyConfirmedPassword(password);

    // Shows signin UI again without changing states.
    Oobe.showScreen({id: SCREEN_GAIA_SIGNIN});
  },

  /**
   * Invoked when the user has successfully authenticated via SAML, the
   * Chrome Credentials Passing API was not used and no passwords
   * could be scraped.
   * The user will be asked to pick a manual password for the device.
   * @param {string} email The authenticated user's e-mail.
   * @private
   */
  onAuthNoPassword_(email) {
    chrome.send('scrapedPasswordCount', [0]);
    login.ConfirmSamlPasswordScreen.show(
        email, true /* manual password entry */,
        this.samlPasswordConfirmAttempt_,
        this.onManualPasswordCollected_.bind(this));
  },

  /**
   * Invoked when the dialog where the user enters a manual password for the
   * device, when password scraping fails.
   * @param {string} password The password the user entered. Not necessarily
   *     the same as their SAML password.
   * @private
   */
  onManualPasswordCollected_(password) {
    this.authenticator_.completeAuthWithManualPassword(password);
  },

  /**
   * Invoked when the authentication flow had to be aborted because content
   * served over an unencrypted connection was detected. Shows a fatal error.
   * This method is only called on Chrome OS, where the entire authentication
   * flow is required to be encrypted.
   * @param {string} url The URL that was blocked.
   * @private
   */
  onInsecureContentBlocked_(url) {
    this.showFatalAuthError_(
        OobeTypes.FatalErrorCode.INSECURE_CONTENT_BLOCKED, {'url': url});
  },

  /**
   * Shows the fatal auth error.
   * @param {OobeTypes.FatalErrorCode} error_code The error code
   * @param {string} info Additional info
   * @private
   */
  showFatalAuthError_(error_code, info) {
    chrome.send('onFatalError', [error_code, info || {}]);
  },

  /**
   * Show fatal auth error when information is missing from GAIA.
   * @private
   */
  missingGaiaInfo_() {
    this.showFatalAuthError_(OobeTypes.FatalErrorCode.MISSING_GAIA_INFO);
  },

  /**
   * Record that SAML API was used during sign-in.
   * @param {boolean} isThirdPartyIdP is login flow SAML with external IdP
   * @private
   */
  samlApiUsed_(isThirdPartyIdP) {
    chrome.send('usingSAMLAPI', [isThirdPartyIdP]);
  },

  /**
   * Record SAML Provider that has signed-in
   * @param {string} X509Certificate is a x509certificate in pem format
   * @private
   */
  recordSAMLProvider_(X509Certificate) {
    chrome.send('recordSAMLProvider', [X509Certificate]);
  },

  /**
   * Invoked when auth is completed successfully.
   * @param {!Object} credentials Credentials of the completed authentication.
   * @private
   */
  onAuthCompleted_(credentials) {
    if (credentials.publicSAML) {
      this.email_ = credentials.email;
      chrome.send('launchSAMLPublicSession', [credentials.email]);
    } else {
      chrome.send('completeAuthentication', [
        credentials.gaiaId, credentials.email, credentials.password,
        credentials.usingSAML, credentials.services,
        credentials.passwordAttributes, credentials.syncTrustedVaultKeys || {}
      ]);
    }

    // Hide the navigation buttons as they are not useful when the loading
    // screen is shown.
    this.navigationButtonsHidden_ = true;

    // Clear any error messages that were shown before login.
    Oobe.clearErrors();

    this.clearVideoTimer_();
    this.authCompleted_ = true;
  },

  /**
   * Invoked when onAuthCompleted message received.
   * @param {!CustomEvent<!Object>} e Event with the credentials object as the
   *     payload.
   * @private
   */
  onAuthCompletedMessage_(e) {
    this.onAuthCompleted_(e.detail);
  },

  /**
   * Invoked when onLoadAbort message received.
   * @param {!CustomEvent<!Object>} e Event with the payload containing
   *     additional information about error event like:
   *     {number} error_code Error code such as net::ERR_INTERNET_DISCONNECTED.
   *     {string} src The URL that failed to load.
   * @private
   */
  onLoadAbortMessage_(e) {
    this.onWebviewError_(e.detail);
  },

  /**
   * Invoked when exit message received.
   * @param {!CustomEvent<!Object>} e Event
   * @private
   */
  onExitMessage_(e) {
    this.cancel();
  },

  /**
   * Invoked when identifierEntered message received.
   * @param {!CustomEvent<!Object>} e Event with payload containing:
   *     {string} accountIdentifier User identifier.
   * @private
   */
  onIdentifierEnteredMessage_(e) {
    this.onIdentifierEntered_(e.detail);
  },

  /**
   * Invoked when removeUserByEmail message received.
   * @param {!CustomEvent<!Object>} e Event with payload containing:
   *     {string} email User email.
   * @private
   */
  onRemoveUserByEmailMessage_(e) {
    this.onRemoveUserByEmail_(e.detail);
  },

  /**
   * Clears input fields and switches to input mode.
   * @param {boolean} takeFocus True to take focus.
   */
  reset(takeFocus) {
    // Reload and show the sign-in UI if needed.
    this.authenticator_.resetStates();
    if (takeFocus) {
      Oobe.getInstance().setOobeUIState(OOBE_UI_STATE.GAIA_SIGNIN);
    }
  },

  /**
   * Reloads extension frame.
   */
  doReload() {
    if (this.screenMode_ != AuthMode.DEFAULT)
      return;
    this.authenticator_.reload();
    this.loadingFrameContents_ = true;
    this.startLoadingTimer_();
    this.authCompleted_ = false;
  },

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attempts tried.
   * @param {HTMLElement} error Content to show in bubble.
   */
  showErrorBubble(loginAttempts, error) {
    if (!this.loadingFrameContents_) {
      $('bubble').showContentForElement(
          this, cr.ui.Bubble.Attachment.BOTTOM, error,
          BUBBLE_HORIZONTAL_PADDING, BUBBLE_VERTICAL_PADDING);
    } else {
      // Defer the bubble until the frame has been loaded.
      this.errorBubble_ = [loginAttempts, error];
    }
  },

  /**
   * Called when user canceled signin.
   */
  cancel(isBackClicked = false) {
    this.clearVideoTimer_();

    // TODO(crbug.com/470893): Figure out whether/which of these exit conditions
    // are useful.
    if (this.isAllowlistErrorShown_ || this.authCompleted_) {
      return;
    }

    this.userActed(isBackClicked ? 'back' : 'cancel');
  },

  /**
   * Handler for webview error handling.
   * @param {!Object} data Additional information about error event like:
   *     {number} error_code Error code such as net::ERR_INTERNET_DISCONNECTED.
   *     {string} src The URL that failed to load.
   * @private
   */
  onWebviewError_(data) {
    chrome.send('webviewLoadAborted', [data.error_code]);
  },

  /**
   * Handler for identifierEntered event.
   * @param {!Object} data The identifier entered by user:
   *     {string} accountIdentifier User identifier.
   * @private
   */
  onIdentifierEntered_(data) {
    chrome.send('identifierEntered', [data.accountIdentifier]);
  },

  /**
   * Handler for removeUserByEmail event.
   * @param {!Object} data The user email:
   *     {string} email User email.
   * @private
   */
  onRemoveUserByEmail_(data) {
    chrome.send('removeUserByEmail', [data]);
    this.cancel();
  },

  /**
   * Show/Hide error when user is not in allowlist. When UI is hidden GAIA is
   * reloaded.
   * @param {boolean} show Show/hide error UI.
   * @param {!Object=} opt_data Optional additional information.
   */
  showAllowlistCheckFailedError(show, opt_data) {
    if (show) {
      const isManaged = opt_data && opt_data.enterpriseManaged;
      const isFamilyLinkAllowed = opt_data && opt_data.familyLinkAllowed;
      errorMessage = '';
      if (isManaged && isFamilyLinkAllowed) {
        errorMessage = 'allowlistErrorEnterpriseAndFamilyLink';
      } else if (isManaged) {
        errorMessage = 'allowlistErrorEnterprise';
      } else {
        errorMessage = 'allowlistErrorConsumer';
      }

      this.$['gaia-allowlist-error'].textContent =
          loadTimeData.getValue(errorMessage);
      // To make animations correct, we need to make sure Gaia is completely
      // reloaded. Otherwise ChromeOS overlays hide and Gaia page is shown
      // somewhere in the middle of animations.
      if (this.screenMode_ == AuthMode.DEFAULT)
        this.authenticator_.resetWebview();
    }

    this.isAllowlistErrorShown_ = show;

    if (show)
      this.$['gaia-allowlist-error'].submitButton.focus();
    else
      Oobe.showSigninUI();
  },

  /**
   * Shows the PIN dialog according to the given parameters.
   *
   * In case the dialog is already shown, updates it according to the new
   * parameters.
   * @param {!OobeTypes.SecurityTokenPinDialogParameters} parameters
   */
  showPinDialog(parameters) {
    assert(parameters);

    // Note that this must be done before updating |pinDialogResultReported_|,
    // since the observer will notify the handler about the cancellation of the
    // previous dialog depending on this flag.
    this.pinDialogParameters_ = parameters;

    this.pinDialogResultReported_ = false;
  },

  /**
   * Closes the PIN dialog (that was previously opened using showPinDialog()).
   * Does nothing if the dialog is not shown.
   */
  closePinDialog() {
    // Note that the update triggers the observer, that notifies the handler
    // about the closing.
    this.pinDialogParameters_ = null;
  },

  /**
   * Observer that is called when the |pinDialogParameters_| property gets
   * changed.
   * @param {OobeTypes.SecurityTokenPinDialogParameter} newValue
   * @param {OobeTypes.SecurityTokenPinDialogParameter} oldValue
   * @private
   */
  onPinDialogParametersChanged_(newValue, oldValue) {
    if (oldValue === undefined) {
      // Don't do anything on the initial call, triggered by the property
      // initialization.
      return;
    }
    if (oldValue === null && newValue !== null) {
      // Asynchronously set the focus, so that this happens after Polymer
      // recalculates the visibility of |pinDialog|.
      // Also notify the C++ test after this happens, in order to avoid
      // flakiness (so that the test doesn't try to simulate the input before
      // the caret is positioned).
      requestAnimationFrame(() => {
        this.$.pinDialog.focus();
        chrome.send('securityTokenPinDialogShownForTest');
      });
    }
    if ((oldValue !== null && newValue === null) ||
        (oldValue !== null && newValue !== null &&
         !this.pinDialogResultReported_)) {
      // Report the cancellation result if the dialog got closed or got reused
      // before reporting the result.
      chrome.send('securityTokenPinEntered', [/*user_input=*/ '']);
    }
  },

  /**
   * Invoked when the user cancels the PIN dialog.
   * @param {!CustomEvent} e
   */
  onPinDialogCanceled_(e) {
    this.closePinDialog();
    this.cancel();
  },

  /**
   * Invoked when the PIN dialog is completed.
   * @param {!CustomEvent<string>} e Event with the entered PIN as the payload.
   */
  onPinDialogCompleted_(e) {
    this.pinDialogResultReported_ = true;
    chrome.send('securityTokenPinEntered', [/*user_input=*/ e.detail]);
  },

  /**
   * Updates current UI step based on internal state.
   * @param {boolean} isScreenShown
   * @param {number} mode
   * @param {OobeTypes.SecurityTokenPinDialogParameter} pinParams
   * @param {boolean} isLoading
   * @param {boolean} isAllowlistError
   * @private
   */
  refreshDialogStep_(
      isScreenShown, mode, pinParams, isLoading, isAllowlistError) {
    if (!isScreenShown)
      return;
    if (pinParams !== null) {
      this.setUIStep(DialogMode.PIN_DIALOG);
      return;
    }
    if (isLoading) {
      if (mode == AuthMode.DEFAULT) {
        this.setUIStep(DialogMode.GAIA_LOADING);
      } else {
        this.setUIStep(DialogMode.LOADING);
      }
      return;
    }
    if (isAllowlistError) {
      this.setUIStep(DialogMode.GAIA_ALLOWLIST_ERROR);
      return;
    }
    switch (mode) {
      case AuthMode.DEFAULT:
        this.setUIStep(DialogMode.GAIA);
        break;
      case AuthMode.SAML_INTERSTITIAL:
        this.setUIStep(DialogMode.SAML_INTERSTITIAL);
        break;
    }
  },

  /**
   * Invoked when "Next" button is pressed on SAML Interstitial screen.
   * @param {!CustomEvent} e
   * @private
   */
  onSamlInterstitialNext_() {
    this.screenMode_ = AuthMode.DEFAULT;
    this.loadAuthenticator_(true /* doSamlRedirect */);
  },

  /**
   * Invoked when "Change account" link is pressed on SAML Interstitial screen.
   * @param {!CustomEvent} e
   * @private
   */
  onSamlPageChangeAccount_() {
    // The user requests to change the account. We must clear the email
    // field of the auth params.
    this.authenticatorParams_.email = '';
    this.screenMode_ = AuthMode.DEFAULT;
    this.loadAuthenticator_(false /* doSamlRedirect */);
  },

  /**
   * Computes the value of the isLoadingUiShown_ property.
   * @param {boolean} loadingFrameContents
   * @param {boolean} isAllowlistErrorShown
   * @param {boolean} authCompleted
   * @return {boolean}
   * @private
   */
  computeIsLoadingUiShown_: function(
      loadingFrameContents, isAllowlistErrorShown, authCompleted) {
    return (loadingFrameContents || authCompleted) && !isAllowlistErrorShown;
  },

  /**
   * Checks if string is empty
   * @param {string} value
   * @private
   */
  isEmpty_(value) {
    return !value;
  },

  clickPrimaryButtonForTesting() {
    this.$['signin-frame-dialog'].clickPrimaryButtonForTesting();
  },

  /**
   * Whether new OOBE layout is enabled.
   */
  newLayoutEnabled_() {
    return loadTimeData.valueExists('newLayoutEnabled') &&
        loadTimeData.getBoolean('newLayoutEnabled');
  },

  /**
   * Called when focus is returned.
   * @param {boolean} reverse Is focus returned in reverse order?
   */
  onFocusReturned(reverse) {
    // We need to explicitly adjust focus inside the webview part when focus is
    // returned from the system tray in regular order. Because the webview is
    // the first focusable element of the screen and we want to eliminate extra
    // tab. Reverse tab doesn't need any adjustments here.
    if (!this.newLayoutEnabled_() && !reverse)
      this.focusActiveFrame_();
  },
});
})();
