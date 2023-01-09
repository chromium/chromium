// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe signin screen implementation.
 */

import '//resources/cr_elements/icons.html.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/notification_card.js';
import '../../components/security_token_pin.js';
import '../../components/gaia_dialog.js';
import '../../components/oobe_icons.m.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_common_styles.m.js';
import '../../components/common_styles/oobe_dialog_host_styles.m.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/throbber_notice.js';

import {assert} from '//resources/ash/common/assert.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AuthFlow, AuthMode, SUPPORTED_PARAMS} from '../../../../gaia_auth_host/authenticator.js';
import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.m.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.m.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';
import {OobeTypes} from '../../components/oobe_types.js';
import {Oobe} from '../../cr_ui.js';
import {invokePolymerMethod} from '../../display_manager.js';


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
const VIDEO_LOGIN_TIMEOUT = 180 * 1000;

/**
 * The authentication mode for the screen.
 * @enum {number}
 */
const ScreenAuthMode = {
  DEFAULT: 0,        // Default GAIA login flow.
  SAML_REDIRECT: 1,  // SAML redirection.
};

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const DialogMode = {
  GAIA: 'online-gaia',
  LOADING: 'loading',
  PIN_DIALOG: 'pin',
  GAIA_ALLOWLIST_ERROR: 'allowlist-error',
};

/**
 * Steps that could be the first one in the flow.
 */
const POSSIBLE_FIRST_SIGNIN_STEPS = [DialogMode.GAIA, DialogMode.LOADING];

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const GaiaSigninElementBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * @polymer
 */
class GaiaSigninElement extends GaiaSigninElementBase {
  static get is() {
    return 'gaia-signin-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
       * Whether the authenticator is currently redirected to |SAML| flow. It is
       * set to true early during a default redirection to address situations
       * when an error during loading the 3P IdP occurs and no change in
       * `authFlow` happens, but the UI for 3P IdP still should be shown.
       * Updated on `authFlow` change.
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
       * Contains the security token PIN dialog parameters object when the
       * dialog is shown. Is null when no PIN dialog is shown.
       * @type {?OobeTypes.SecurityTokenPinDialogParameters}
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
       * Bound to gaia-dialog::authDomain.
       * @private
       */
      authDomain_: {
        type: String,
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
        observer: 'onIsFirstSigninStepChanged',
      },

      /*
       * Whether the screen is shown.
       * @private
       */
      isShown_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the default SAML 3rd-party page is configured for the device.
       * @private
       */
      isDefaultSsoProviderConfigured_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the default SAML 3rd-party page is visible.
       * @private
       */
      isDefaultSsoProvider_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the screen can be hidden.
       */
      isClosable_: {
        type: Boolean,
        value: false,
      },

      /**
       * @private {string}
       */
      allowlistError_: {
        type: String,
        value: 'allowlistErrorConsumer',
      },
    };
  }

  constructor() {
    super();
    /**
     * Saved authenticator load params.
     * @type {?Object}
     * @private
     */
    this.authenticatorParams_ = null;

    /**
     * Email of the user, which is logging in using offline mode.
     * @type {string}
     * @private
     */
    this.email_ = '';

    /**
     * Timer id of pending load.
     * @type {number|undefined}
     * @private
     */
    this.loadingTimer_ = undefined;

    /**
     * Timer id of a guard timer that is fired in case 'showView' message is not
     * received from GAIA.
     * @type {number|undefined}
     * @private
     */
    this.loadAnimationGuardTimer_ = undefined;

    /**
     * Timer id of the video login timer.
     * @type {number|undefined}
     * @private
     */
    this.videoTimer_ = undefined;

    /**
     * Whether we've processed 'showView' message - either from GAIA or from
     * guard timer.
     * @type {boolean}
     * @private
     */
    this.showViewProcessed_ = false;

    /**
     * Whether we've processed 'authCompleted' message.
     * @type {boolean}
     * @private
     */
    this.authCompleted_ = false;

    /**
     * Whether the result was reported to the handler for the most recent PIN
     * dialog.
     * @type {boolean}
     * @private
     */
    this.pinDialogResultReported_ = false;
  }

  get EXTERNAL_API() {
    return [
      'loadAuthExtension',
      'doReload',
      'showAllowlistCheckFailedError',
      'showPinDialog',
      'closePinDialog',
      'clickPrimaryButtonForTesting',
      'onBeforeLoad',
      'reset',
    ];
  }

  static get observers() {
    return [
      'refreshDialogStep_(isShown_, pinDialogParameters_,' +
          'isLoadingUiShown_, isAllowlistErrorShown_)',
    ];
  }

  defaultUIStep() {
    return DialogMode.GAIA;
  }

  get UI_STEPS() {
    return DialogMode;
  }

  get authenticator_() {
    return this.$['signin-frame-dialog'].getAuthenticator();
  }

  /** @override */
  ready() {
    super.ready();
    this.authenticator_.insecureContentBlockedCallback =
        this.onInsecureContentBlocked_.bind(this);
    this.authenticator_.missingGaiaInfoCallback =
        this.missingGaiaInfo_.bind(this);
    this.authenticator_.samlApiUsedCallback = this.samlApiUsed_.bind(this);
    this.authenticator_.recordSAMLProviderCallback =
        this.recordSAMLProvider_.bind(this);

    this.initializeLoginScreen('GaiaSigninScreen');
  }

  /**
   * Updates whether the Guest and Apps button is allowed to be shown. (Note
   * that the C++ side contains additional logic that decides whether the
   * Guest button should be shown.)
   * @private
   */
  isFirstSigninStep(uiStep, canGaiaGoBack, isSaml) {
    return !this.isClosable_ && POSSIBLE_FIRST_SIGNIN_STEPS.includes(uiStep) &&
        !canGaiaGoBack && !(isSaml && !this.isDefaultSsoProvider_);
  }

  onIsFirstSigninStepChanged(isFirstSigninStep) {
    if (this.isShown_) {
      chrome.send('setIsFirstSigninStep', [isFirstSigninStep]);
    }
  }

  /**
   * Handles clicks on "Back" button.
   * @private
   */
  onBackButtonCancel_() {
    if (!this.authCompleted_) {
      this.cancel(true /* isBackClicked */);
    }
  }

  onInterstitialBackButtonClicked_() {
    this.cancel(true /* isBackClicked */);
  }

  /**
   * Handles user closes the dialog on the SAML page.
   * @private
   */
  closeSaml_() {
    this.videoEnabled_ = false;
    this.cancel(false /* isBackClicked */);
  }

  /**
   * Loads the authenticator and updates the UI to reflect the loading state.
   * @param {boolean} doSamlRedirect If the authenticator should do
   *     authentication by automatic redirection to the SAML-based enrollment
   *     enterprise domain IdP.
   * @private
   */
  loadAuthenticator_(doSamlRedirect) {
    this.loadingFrameContents_ = true;
    this.isAllowlistErrorShown_ = false;
    this.isDefaultSsoProvider_ = doSamlRedirect;
    this.startLoadingTimer_();

    // Don't enable GAIA action buttons when doing SAML redirect.
    this.authenticatorParams_.enableGaiaActionButtons = !doSamlRedirect;
    this.authenticatorParams_.doSamlRedirect = doSamlRedirect;
    // Set `isSaml_` flag here to make sure we show the correct UI. If we do a
    // redirect, but it fails due to an error, there won't be any `authFlow`
    // change triggered by `authenticator_`, however we should still show a
    // button to let user go to GAIA page and keep original GAIA buttons
    // hidden.
    this.isSaml_ = doSamlRedirect;
    this.authenticator_.load(AuthMode.DEFAULT, this.authenticatorParams_);
  }

  /**
   * Whether the SAML 3rd-party page is visible.
   * @param {boolean} isSaml
   * @param {OobeTypes.SecurityTokenPinDialogParameters} pinDialogParameters
   * @return {boolean}
   * @private
   */
  computeSamlSsoVisible_(isSaml, pinDialogParameters) {
    return isSaml && !pinDialogParameters;
  }

  /**
   * Handler for Gaia loading timeout.
   * @private
   */
  onLoadingTimeOut_() {
    if (Oobe.getInstance().currentScreen.id != 'gaia-signin') {
      return;
    }
    this.loadingTimer_ = undefined;
    chrome.send('showLoadingTimeoutError');
  }

  /**
   * Clears loading timer.
   * @private
   */
  clearLoadingTimer_() {
    if (this.loadingTimer_) {
      clearTimeout(this.loadingTimer_);
      this.loadingTimer_ = undefined;
    }
  }

  /**
   * Sets up loading timer.
   * @private
   */
  startLoadingTimer_() {
    this.clearLoadingTimer_();
    this.loadingTimer_ = setTimeout(
        this.onLoadingTimeOut_.bind(this), MAX_GAIA_LOADING_TIME_SEC * 1000);
  }

  /**
   * Handler for GAIA animation guard timer.
   * @private
   */
  onLoadAnimationGuardTimer_() {
    this.loadAnimationGuardTimer_ = undefined;
    this.onShowView_();
  }

  /**
   * Clears GAIA animation guard timer.
   * @private
   */
  clearLoadAnimationGuardTimer_() {
    if (this.loadAnimationGuardTimer_) {
      clearTimeout(this.loadAnimationGuardTimer_);
      this.loadAnimationGuardTimer_ = undefined;
    }
  }

  /**
   * Sets up GAIA animation guard timer.
   * @private
   */
  startLoadAnimationGuardTimer_() {
    this.clearLoadAnimationGuardTimer_();
    this.loadAnimationGuardTimer_ = setTimeout(
        this.onLoadAnimationGuardTimer_.bind(this),
        GAIA_ANIMATION_GUARD_MILLISEC);
  }

  getOobeUIInitialState() {
    return OOBE_UI_STATE.GAIA_SIGNIN;
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {Object} data Screen init payload
   */
  onBeforeShow(data) {
    // Re-enable navigation in case it was disabled before refresh.
    this.navigationEnabled_ = true;

    this.isShown_ = true;

    if (data && 'hasUserPods' in data) {
      this.isClosable_ = data.hasUserPods;
    }

    invokePolymerMethod(this.$.pinDialog, 'onBeforeShow');
  }

  /** @private */
  getSigninFrame_() {
    return this.$['signin-frame-dialog'].getFrame();
  }

  /** @private */
  focusSigninFrame_() {
    const signinFrame = this.getSigninFrame_();
    afterNextRender(this, () => signinFrame.focus());
  }

  /** Event handler that is invoked after the screen is shown. */
  onAfterShow() {
    if (!this.isLoadingUiShown_) {
      this.focusSigninFrame_();
    }
  }

  /**
   * Event handler that is invoked just before the screen is hidden.
   */
  onBeforeHide() {
    this.isShown_ = false;
    this.authenticator_.resetWebview();
  }

  /**
   * Loads the authentication extension into the iframe.
   * @param {!Object} data Extension parameters bag.
   * @suppress {missingProperties}
   */
  loadAuthExtension(data) {
    this.authenticator_.setWebviewPartition(data.webviewPartitionName);

    this.authCompleted_ = false;
    this.navigationButtonsHidden_ = false;

    // Reset SAML
    this.isSaml_ = false;
    this.usedSaml_ = false;

    // Reset the PIN dialog, in case it's shown.
    this.closePinDialog();

    const params = {};
    SUPPORTED_PARAMS.forEach(name => {
      if (data.hasOwnProperty(name)) {
        params[name] = data[name];
      }
    });

    this.isDefaultSsoProviderConfigured_ =
        data.screenMode == ScreenAuthMode.SAML_REDIRECT;
    params.doSamlRedirect = data.screenMode == ScreenAuthMode.SAML_REDIRECT;
    params.menuEnterpriseEnrollment =
        !(data.enterpriseManagedDevice || data.hasDeviceOwner);
    params.isFirstUser = !(data.enterpriseManagedDevice || data.hasDeviceOwner);
    params.obfuscatedOwnerId = data.obfuscatedOwnerId;

    this.authenticatorParams_ = params;

    this.loadAuthenticator_(params.doSamlRedirect);
    chrome.send('authExtensionLoaded');
  }

  /**
   * Whether the current auth flow is SAML.
   * @return {boolean}
   */
  isSamlAuthFlowForTesting() {
    return this.isSaml_ && this.authFlow == AuthFlow.SAML;
  }

  /**
   * Clean up from a video-enabled SAML flow.
   * @private
   */
  clearVideoTimer_() {
    if (this.videoTimer_ !== undefined) {
      clearTimeout(this.videoTimer_);
      this.videoTimer_ = undefined;
    }
  }

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
  }

  reset() {
    this.clearLoadingTimer_();
    this.clearVideoTimer_();
    this.isAllowlistErrorShown_ = false;
    this.authCompleted_ = false;
    // Reset webview to prevent calls from authenticator.
    this.authenticator_.resetWebview();
    this.authenticator_.resetStates();
    // Explicitly disable video here to let `onVideoEnabledChange_()` handle
    // timer start next time when `videoEnabled_` will be set to true on SAML
    // page.
    this.videoEnabled_ = false;
  }

  /**
   * Invoked when the authFlow property is changed on the authenticator.
   * @private
   */
  onAuthFlowChange_() {
    this.isSaml_ = this.authFlow == AuthFlow.SAML;
  }

  /**
   * Observer that is called when the |isSaml_| property gets changed.
   * @param {number} newValue
   * @param {number} oldValue
   * @private
   */
  onSamlChanged_(newValue, oldValue) {
    if (this.isSaml_) {
      this.usedSaml_ = true;
    }

    chrome.send('samlStateChanged', [this.isSaml_]);

    this.classList.toggle('saml', this.isSaml_);
  }

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
    setTimeout(function() {
      this.loadingFrameContents_ = false;
    }.bind(this), 100);
  }

  /**
   * @private
   */
  onStartEnrollment_() {
    this.userActed('startEnrollment');
  }

  /**
   * Invoked when the authenticator emits 'showView' event or when corresponding
   * guard time fires.
   * @private
   */
  onShowView_() {
    if (this.showViewProcessed_) {
      return;
    }

    this.showViewProcessed_ = true;
    this.clearLoadAnimationGuardTimer_();
    this.onLoginUIVisible_();
  }

  /**
   * Called when UI is shown.
   * @private
   */
  onLoginUIVisible_() {
    chrome.send('loginWebuiReady');
  }

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
  }

  /**
   * Shows the fatal auth error.
   * @param {OobeTypes.FatalErrorCode} error_code The error code
   * @param {Object} [info] Additional info
   * @private
   */
  showFatalAuthError_(error_code, info) {
    chrome.send('onFatalError', [error_code, info || {}]);
  }

  /**
   * Show fatal auth error when information is missing from GAIA.
   * @private
   */
  missingGaiaInfo_() {
    this.showFatalAuthError_(OobeTypes.FatalErrorCode.MISSING_GAIA_INFO);
  }

  /**
   * Record that SAML API was used during sign-in.
   * @param {boolean} isThirdPartyIdP is login flow SAML with external IdP
   * @private
   */
  samlApiUsed_(isThirdPartyIdP) {
    chrome.send('usingSAMLAPI', [isThirdPartyIdP]);
  }

  /**
   * Record SAML Provider that has signed-in
   * @param {string} X509Certificate is a x509certificate in pem format
   * @private
   */
  recordSAMLProvider_(X509Certificate) {
    chrome.send('recordSAMLProvider', [X509Certificate]);
  }

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
        credentials.gaiaId,
        credentials.email,
        credentials.password,
        credentials.scrapedSAMLPasswords,
        credentials.usingSAML,
        credentials.services,
        credentials.servicesProvided,
        credentials.passwordAttributes,
        credentials.syncTrustedVaultKeys || {},
      ]);
    }

    // Hide the navigation buttons as they are not useful when the loading
    // screen is shown.
    this.navigationButtonsHidden_ = true;

    this.clearVideoTimer_();
    this.authCompleted_ = true;
  }

  /**
   * Invoked when onAuthCompleted message received.
   * @param {!CustomEvent<!Object>} e Event with the credentials object as the
   *     payload.
   * @private
   */
  onAuthCompletedMessage_(e) {
    this.onAuthCompleted_(e.detail);
  }

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
  }

  /**
   * Invoked when exit message received.
   * @param {!CustomEvent<!Object>} e Event
   * @private
   */
  onExitMessage_(e) {
    this.cancel();
  }

  /**
   * Invoked when identifierEntered message received.
   * @param {!CustomEvent<!Object>} e Event with payload containing:
   *     {string} accountIdentifier User identifier.
   * @private
   */
  onIdentifierEnteredMessage_(e) {
    this.onIdentifierEntered_(e.detail);
  }

  /**
   * Invoked when removeUserByEmail message received.
   * @param {!CustomEvent<!Object>} e Event with payload containing:
   *     {string} email User email.
   * @private
   */
  onRemoveUserByEmailMessage_(e) {
    this.onRemoveUserByEmail_(e.detail);
  }

  /**
   * Reloads extension frame.
   */
  doReload() {
    this.authenticator_.reload();
    this.loadingFrameContents_ = true;
    this.isAllowlistErrorShown_ = false;
    this.startLoadingTimer_();
    this.authCompleted_ = false;
  }

  /**
   * Called when user canceled signin. If it is default SAML page we try to
   * close the page. If SAML page was derived from GAIA we return to configured
   * default page (GAIA or default SAML page).
   */
  cancel(isBackClicked = false) {
    this.clearVideoTimer_();

    // TODO(crbug.com/470893): Figure out whether/which of these exit conditions
    // are useful.
    if (this.isAllowlistErrorShown_ || this.authCompleted_) {
      return;
    }

    // If user goes back from the derived SAML page or GAIA page that is shown
    // to change SSO provider we need to reload default authenticator.
    if ((this.isSamlSsoVisible_ || this.isDefaultSsoProviderConfigured_) &&
        !this.isDefaultSsoProvider_) {
      this.userActed('reloadDefault');
      return;
    }
    this.userActed(isBackClicked ? 'back' : 'cancel');
  }

  /**
   * Handler for webview error handling.
   * @param {!Object} data Additional information about error event like:
   *     {number} error_code Error code such as net::ERR_INTERNET_DISCONNECTED.
   *     {string} src The URL that failed to load.
   * @private
   */
  onWebviewError_(data) {
    chrome.send('webviewLoadAborted', [data.error_code]);
  }

  /**
   * Handler for identifierEntered event.
   * @param {!Object} data The identifier entered by user:
   *     {string} accountIdentifier User identifier.
   * @private
   */
  onIdentifierEntered_(data) {
    chrome.send('identifierEntered', [data.accountIdentifier]);
  }

  /**
   * Handler for removeUserByEmail event.
   * @param {!Object} data The user email:
   *     {string} email User email.
   * @private
   */
  onRemoveUserByEmail_(data) {
    chrome.send('removeUserByEmail', [data]);
    this.cancel();
  }

  /**
   * Show/Hide error when user is not in allowlist. When UI is hidden GAIA is
   * reloaded.
   * @param {!Object=} opt_data Optional additional information.
   */
  showAllowlistCheckFailedError(opt_data) {
    const isManaged = opt_data && opt_data.enterpriseManaged;
    const isFamilyLinkAllowed = opt_data && opt_data.familyLinkAllowed;
    if (isManaged && isFamilyLinkAllowed) {
      this.allowlistError_ = 'allowlistErrorEnterpriseAndFamilyLink';
    } else if (isManaged) {
      this.allowlistError_ = 'allowlistErrorEnterprise';
    } else {
      this.allowlistError_ = 'allowlistErrorConsumer';
    }

    // Reset the state when allowlist error is shown to prevent any loading
    // glitches during the retry.
    this.reset();

    this.$['gaia-allowlist-error'].submitButton.focus();
    this.isAllowlistErrorShown_ = true;
  }

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
  }

  /**
   * Closes the PIN dialog (that was previously opened using showPinDialog()).
   * Does nothing if the dialog is not shown.
   */
  closePinDialog() {
    // Note that the update triggers the observer, that notifies the handler
    // about the closing.
    this.pinDialogParameters_ = null;
  }

  /**
   * Observer that is called when the |pinDialogParameters_| property gets
   * changed.
   * @param {OobeTypes.SecurityTokenPinDialogParameters} newValue
   * @param {OobeTypes.SecurityTokenPinDialogParameters} oldValue
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
  }

  /**
   * Invoked when the user cancels the PIN dialog.
   * @param {!CustomEvent} e
   */
  onPinDialogCanceled_(e) {
    this.closePinDialog();
    this.cancel();
  }

  /**
   * Invoked when the PIN dialog is completed.
   * @param {!CustomEvent<string>} e Event with the entered PIN as the payload.
   */
  onPinDialogCompleted_(e) {
    this.pinDialogResultReported_ = true;
    chrome.send('securityTokenPinEntered', [/*user_input=*/ e.detail]);
  }

  /**
   * Updates current UI step based on internal state.
   * @param {boolean} isScreenShown
   * @param {OobeTypes.SecurityTokenPinDialogParameters} pinParams
   * @param {boolean} isLoading
   * @param {boolean} isAllowlistError
   * @private
   */
  refreshDialogStep_(isScreenShown, pinParams, isLoading, isAllowlistError) {
    if (!isScreenShown) {
      return;
    }
    if (pinParams !== null) {
      this.setUIStep(DialogMode.PIN_DIALOG);
      return;
    }
    if (isLoading) {
      this.setUIStep(DialogMode.LOADING);
      return;
    }
    if (isAllowlistError) {
      this.setUIStep(DialogMode.GAIA_ALLOWLIST_ERROR);
      return;
    }
    this.setUIStep(DialogMode.GAIA);
  }

  /**
   * Invoked when "Enter Google Account info" button is pressed on SAML screen.
   * @param {!CustomEvent} e
   * @private
   */
  onSamlPageChangeAccount_(e) {
    // The user requests to change the account. We must clear the email
    // field of the auth params.
    this.videoEnabled_ = false;
    this.authenticatorParams_.email = '';
    this.loadAuthenticator_(false /* doSamlRedirect */);
  }

  /**
   * Computes the value of the isLoadingUiShown_ property.
   * @param {boolean} loadingFrameContents
   * @param {boolean} isAllowlistErrorShown
   * @param {boolean} authCompleted
   * @return {boolean}
   * @private
   */
  computeIsLoadingUiShown_(
      loadingFrameContents, isAllowlistErrorShown, authCompleted) {
    return (loadingFrameContents || authCompleted) && !isAllowlistErrorShown;
  }

  /**
   * Checks if string is empty
   * @param {string} value
   * @private
   */
  isEmpty_(value) {
    return !value;
  }

  clickPrimaryButtonForTesting() {
    this.$['signin-frame-dialog'].clickPrimaryButtonForTesting();
  }

  onBeforeLoad() {
    // TODO(https://crbug.com/1317991): Investigate why the call is making Gaia
    // loading slowly.
    // this.loadingFrameContents_ = true;
    // this.isAllowlistErrorShown_ = false;
  }

  /**
   * Copmose alert message when third-party IdP uses camera for authentication.
   * @param {string} locale  i18n locale data
   * @param {boolean} videoEnabled
   * @param {string} authDomain
   * @return {string}
   * @private
   */
  getSamlVideoAlertMessage_(locale, videoEnabled, authDomain) {
    if (videoEnabled && authDomain) {
      return this.i18n('samlNoticeWithVideo', authDomain);
    }
    return '';
  }

  onAllowlistErrorTryAgainClick_() {
    this.userActed('retry');
  }

  onAllowlistErrorLinkClick_() {
    chrome.send('launchHelpApp', [HELP_CANT_ACCESS_ACCOUNT]);
  }
}

customElements.define(GaiaSigninElement.is, GaiaSigninElement);
