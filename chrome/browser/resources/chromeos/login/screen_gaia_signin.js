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

// Amount of time the user has to be idle for before showing the online login
// page.
const IDLE_TIME_UNTIL_EXIT_OFFLINE_IN_MILLISECONDS = 180 * 1000;

// Approximate amount of time between checks to see if we should go to the
// online login page when we're in the offline login page and the device is
// online.
const IDLE_TIME_CHECK_FREQUENCY = 5 * 1000;

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
  OFFLINE: 1,            // GAIA offline login.
  SAML_INTERSTITIAL: 2,  // Interstitial page before SAML redirection.
  AD_AUTH: 3             // Offline Active Directory login flow.
};

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const DialogMode = {
  GAIA: 'online-gaia',
  OFFLINE_GAIA: 'offline-gaia',
  OFFLINE_AD: 'ad',
  GAIA_LOADING: 'gaia-loading',
  LOADING: 'loading',
  PIN_DIALOG: 'pin',
  GAIA_ALLOWLIST_ERROR: 'allowlist-error',
  SAML_INTERSTITIAL: 'saml-interstitial',
};

Polymer({
  is: 'gaia-signin',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  EXTERNAL_API: [
    'loadAuthExtension',
    'doReload',
    'monitorOfflineIdle',
    'showAllowlistCheckFailedError',
    'invalidateAd',
    'showPinDialog',
    'closePinDialog',
  ],

  properties: {

    /**
     * Current mode of this screen.
     * @private
     */
    screenMode_: {
      type: Number,
      value: AuthMode.DEFAULT,
      observer: 'screenModeChanged_',
    },

    /**
     * Current step displayed.
     * @type {DialogMode}
     * @private
     */
    step_: {
      type: String,
      value: DialogMode.GAIA,
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
     * Controls label on the primary action button.
     * @private
     */
    primaryActionButtonLabel_: {
      type: String,
      value: null,
    },

    /**
     * Controls availability of the primary action button.
     * @private
     */
    primaryActionButtonEnabled_: {
      type: Boolean,
      value: true,
    },

    /**
     * Controls label on the secondary action button.
     * @private
     */
    secondaryActionButtonLabel_: {
      type: String,
      value: null,
    },

    /**
     * Controls availability of the secondary action button.
     * @private
     */
    secondaryActionButtonEnabled_: {
      type: Boolean,
      value: true,
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
     * Whether a pop-up overlay should be shown. This overlay is necessary
     * when GAIA shows an overlay within their iframe. It covers the parts
     * of the screen that would otherwise not show an overlay.
     */
    isPopUpOverlayVisible_: {
      type: Boolean,
      computed: 'showOverlay_(navigationEnabled_, isSamlSsoVisible_)'
    }
  },

  observers: [
    'refreshDialogStep_(screenMode_, pinDialogParameters_, isLoadingUiShown_,' +
        'isAllowlistErrorShown_)',
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
   * Value contained in the last received 'backButton' event.
   * @type {boolean}
   * @private
   */
  lastBackMessageValue_: false,

  /**
   * SAML password confirmation attempt count.
   * @type {number}
   * @private
   */
  samlPasswordConfirmAttempt_: 0,

  /**
   * Do we currently have a setTimeout task running that tries to bring us
   * back to the online login page after the user has idled for awhile? If so,
   * then this id will be non-negative.
   * @type {number}
   * @private
   */
  tryToGoToOnlineLoginPageCallbackId_: -1,

  /**
   * The most recent period of time that the user has interacted. This is only
   * updated when the offline page is active and the device is online.
   * @type {number}
   * @private
   */
  mostRecentUserActivity_: Date.now(),

  /**
   * The UI component that hosts IdP pages.
   * @type {!cr.login.Authenticator|undefined}
   */
  authenticator_: undefined,

  /**
   * Whether the result was reported to the handler for the most recent PIN
   * dialog.
   * @type {boolean}
   * @private
   */
  pinDialogResultReported_: false,

  /**
   * Emulate click on the primary action button when it is visible and enabled.
   * @type {boolean}
   * @private
   */
  clickPrimaryActionButtonForTesting_: false,

  /** @override */
  ready() {
    this.authenticator_ = new cr.login.Authenticator(this.getSigninFrame_());

    const that = this;
    const $that = this.$;
    [this.authenticator_, this.$['offline-gaia'], this.$['offline-ad-auth']]
        .forEach(function(frame) {
          // Ignore events from currently inactive frame.
          const frameFilter = function(callback) {
            return function(e) {
              let currentFrame = null;
              switch (that.screenMode_) {
                case AuthMode.DEFAULT:
                case AuthMode.SAML_INTERSTITIAL:
                  currentFrame = that.authenticator_;
                  break;
                case AuthMode.OFFLINE:
                  currentFrame = $that['offline-gaia'];
                  break;
                case AuthMode.AD_AUTH:
                  currentFrame = $that['offline-ad-auth'];
                  break;
              }
              if (frame === currentFrame)
                callback.call(that, e);
            };
          };

          frame.addEventListener(
              'authCompleted', frameFilter(that.onAuthCompletedMessage_));
          frame.addEventListener('backButton', frameFilter(that.onBackButton_));
          frame.addEventListener(
              'dialogShown', frameFilter(that.onDialogShown_));
          frame.addEventListener(
              'dialogHidden', frameFilter(that.onDialogHidden_));
          frame.addEventListener(
              'menuItemClicked', frameFilter(that.onMenuItemClicked_));
        });

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

    /**
     * Event listeners for the events triggered by the authenticator.
     */
    const authenticatorEventListeners = {
      'authDomainChange': this.onAuthDomainChange_,
      'authFlowChange': this.onAuthFlowChange_,
      'identifierEntered': this.onIdentifierEnteredMessage_,
      'loadAbort': this.onLoadAbortMessage_,
      'ready': this.onAuthReady_,
      'setPrimaryActionEnabled': this.onSetPrimaryActionEnabled_,
      'setPrimaryActionLabel': this.onSetPrimaryActionLabel_,
      'setSecondaryActionEnabled': this.onSetSecondaryActionEnabled_,
      'setSecondaryActionLabel': this.onSetSecondaryActionLabel_,
      'setAllActionsEnabled': this.onSetAllActionsEnabled_,
      'showView': (e) => {
        // Redirect to onShowView_() with dropping the |e| argument.
        this.onShowView_();
      },
      'videoEnabledChange': this.onVideoEnabledChange_,
    };
    for (eventName in authenticatorEventListeners) {
      this.authenticator_.addEventListener(
          eventName, authenticatorEventListeners[eventName].bind(this));
    }

    this.$['offline-gaia'].addEventListener(
        'offline-gaia-cancel', this.cancel.bind(this));

    this.$['gaia-allowlist-error'].addEventListener('buttonclick', function() {
      this.showAllowlistCheckFailedError(false);
    }.bind(this));

    this.$['gaia-allowlist-error'].addEventListener('linkclick', function() {
      chrome.send('launchHelpApp', [HELP_CANT_ACCESS_ACCOUNT]);
    });

    this.$['offline-ad-auth'].addEventListener('cancel', function() {
      this.cancel();
    }.bind(this));

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
    return Oobe.getInstance().hasUserPods || this.isOffline_();
  },

  /**
   * Returns true if the screen is at the beginning of flow (i.e. the email
   * page).
   * @type {boolean}
   * @private
   */
  isAtTheBeginning_() {
    return !this.canGoBack_() && !this.isSaml_ &&
        !this.isAllowlistErrorShown_ && !this.authCompleted_;
  },

  /**
   * Updates whether the Guest button is allowed to be shown. (Note that the
   * C++ side contains additional logic that decides whether the Guest button
   * should be shown.)
   * @private
   */
  updateGuestButtonVisibility_() {
    let showGuestInOobe = !this.isClosable_() && this.isAtTheBeginning_();
    // TODO(rsorokin): Rename message string to reflect the meaning.
    chrome.send('showGuestInOobe', [showGuestInOobe]);
  },

  /**
   * Handles clicks on "PrimaryAction" button.
   */
  onPrimaryActionButtonClicked_() {
    this.authenticator_.sendMessageToWebview('primaryActionHit');
  },

  /**
   * Handles clicks on "SecondaryAction" button.
   */
  onSecondaryActionButtonClicked_() {
    this.authenticator_.sendMessageToWebview('secondaryActionHit');
  },

  /**
   * Returns whether it's possible to rewind the sign-in frame one step back (as
   * opposed to cancelling the sign-in flow).
   * @type {boolean}
   * @private
   */
  canGoBack_() {
    return this.lastBackMessageValue_ && !this.isAllowlistErrorShown_ &&
        !this.authCompleted_ && !this.isSaml_;
  },

  /**
   * Handles clicks on "Back" button.
   * @private
   */
  onBackButtonClicked_() {
    if (!this.canGoBack_()) {
      this.cancel();
    } else {
      this.getActiveFrame_().back();
    }
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
   * Returns true if offline version of Gaia is used.
   * @return {boolean}
   * @private
   */
  isOffline_() {
    return this.screenMode_ == AuthMode.OFFLINE;
  },

  /**
   * Observer that is called when the |screenMode_| property gets changed.
   * @param {number} newValue
   * @param {number} oldValue
   * @private
   */
  screenModeChanged_(newValue, oldValue) {
    if (oldValue === undefined) {
      // Ignore the first call, triggered by the assignment of the initial
      // value.
      return;
    }
    chrome.send('updateOfflineLogin', [this.isOffline_()]);
    this.updateGuestButtonVisibility_();
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
   * This enables or disables trying to go back to the online login page
   * after the user is idle for a few minutes, assuming that we're currently
   * in the offline one. This is only applicable when the offline page is
   * currently active. It is intended that when the device goes online, this
   * gets called with true; when it goes offline, this gets called with
   * false.
   * @param {boolean} shouldMonitor
   */
  monitorOfflineIdle(shouldMonitor) {
    const ACTIVITY_EVENTS = ['click', 'mousemove', 'keypress'];
    const self = this;

    // updateActivityTime_ is used as a callback for addEventListener, so we
    // need the exact reference for removeEventListener. Because the callback
    // needs to access the |this| as scoped inside of this function, we create
    // a closure that uses the appropriate |this|.
    //
    // Unfortunately, we cannot define this function inside of the JSON object
    // as then we have no way to create to capture the correct |this| reference.
    // We define it here instead.
    if (!self.updateActivityTime_) {
      self.updateActivityTime_ = function() {
        self.mostRecentUserActivity_ = Date.now();
      };
    }

    // Begin monitoring.
    if (shouldMonitor) {
      // If we're not using the offline login page or we're already
      // monitoring, then we don't need to do anything.
      if (!self.isOffline_() ||
          self.tryToGoToOnlineLoginPageCallbackId_ !== -1) {
        return;
      }

      self.mostRecentUserActivity_ = Date.now();
      ACTIVITY_EVENTS.forEach(function(event) {
        document.addEventListener(event, self.updateActivityTime_);
      });

      self.tryToGoToOnlineLoginPageCallbackId_ = setInterval(function() {
        // If we're not in the offline page or the signin page, then we want
        // to terminate monitoring.
        if (!self.isOffline_() ||
            Oobe.getInstance().currentScreen.id != 'gaia-signin') {
          self.monitorOfflineIdle(false);
          return;
        }

        const idleDuration = Date.now() - self.mostRecentUserActivity_;
        if (idleDuration > IDLE_TIME_UNTIL_EXIT_OFFLINE_IN_MILLISECONDS) {
          self.monitorOfflineIdle(false);
          Oobe.resetSigninUI(true);
        }
      }, IDLE_TIME_CHECK_FREQUENCY);
    }

    // Stop monitoring.
    else {
      // We're not monitoring, so we don't need to do anything.
      if (self.tryToGoToOnlineLoginPageCallbackId_ === -1)
        return;

      ACTIVITY_EVENTS.forEach(function(event) {
        document.removeEventListener(event, self.updateActivityTime_);
      });
      clearInterval(self.tryToGoToOnlineLoginPageCallbackId_);
      self.tryToGoToOnlineLoginPageCallbackId_ = -1;
    }
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

    this.lastBackMessageValue_ = false;
    this.updateGuestButtonVisibility_();

    cr.ui.login.invokePolymerMethod(this.$['offline-ad-auth'], 'onBeforeShow');
    cr.ui.login.invokePolymerMethod(
        this.$['signin-frame-dialog'], 'onBeforeShow');
    cr.ui.login.invokePolymerMethod(this.$['offline-gaia'], 'onBeforeShow');
    cr.ui.login.invokePolymerMethod(this.$.pinDialog, 'onBeforeShow');
  },

  /**
   * @return {!Element}
   * @private
   */
  getSigninFrame_() {
    // Note: Can't use |this.$|, since it returns cached references to elements
    // originally present in DOM, while the signin-frame is dynamically
    // recreated (see Authenticator.setWebviewPartition()).
    const signinFrame = this.shadowRoot.getElementById('signin-frame');
    assert(signinFrame);
    return signinFrame;
  },

  /** @private */
  getActiveFrame_() {
    switch (this.screenMode_) {
      case AuthMode.DEFAULT:
        return this.getSigninFrame_();
      case AuthMode.OFFLINE:
        return this.$['offline-gaia'];
      case AuthMode.AD_AUTH:
        return this.$['offline-ad-auth'];
      case AuthMode.SAML_INTERSTITIAL:
        return this.$['saml-interstitial'];
    }
  },

  /** @private */
  focusActiveFrame_() {
    this.getActiveFrame_().focus();
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
    this.$['offline-gaia'].switchToEmailCard(false /* animated */);
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
    this.email_ = '';
    this.authCompleted_ = false;
    this.lastBackMessageValue_ = false;
    this.setBackNavigationVisibility_(true);

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
    params.enableGaiaActionButtons = data.enableGaiaActionButtons;

    this.authenticatorParams_ = params;

    switch (this.screenMode_) {
      case AuthMode.DEFAULT:
        this.loadAuthenticator_(false /* doSamlRedirect */);
        break;

      case AuthMode.OFFLINE:
        this.loadOffline_(params);
        break;

      case AuthMode.AD_AUTH:
        this.loadAdAuth_(params);
        break;

      case AuthMode.SAML_INTERSTITIAL:
        this.samlInterstitialDomain_ = data.enterpriseDisplayDomain;
        this.loadingFrameContents_ = false;
        break;
    }
    this.updateGuestButtonVisibility_();
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
   * Helper function to update the title bar.
   * @private
   */
  updateSamlNotice_() {
    if (this.authenticator_.videoEnabled) {
      this.$['saml-notice-message'].textContent = loadTimeData.getStringF(
          'samlNoticeWithVideo', this.authenticator_.authDomain);
      this.$['saml-notice-recording-indicator'].hidden = false;
      this.$['saml-notice-container'].style.justifyContent = 'flex-start';
    } else {
      this.$['saml-notice-message'].textContent =
          loadTimeData.getStringF('samlNotice', this.authenticator_.authDomain);
      this.$['saml-notice-recording-indicator'].hidden = true;
      this.$['saml-notice-container'].style.justifyContent = 'center';
    }
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
   * Invoked when the authDomain property is changed on the authenticator.
   * @private
   */
  onAuthDomainChange_() {
    this.updateSamlNotice_();
  },

  /**
   * Invoked when the videoEnabled property is changed on the authenticator.
   * @private
   */
  onVideoEnabledChange_() {
    this.updateSamlNotice_();
    if (this.authenticator_.videoEnabled && this.videoTimer_ === undefined) {
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
    this.isSaml_ =
        this.authenticator_.authFlow == cr.login.Authenticator.AuthFlow.SAML;
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

      this.updateGuestButtonVisibility_();
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

      if (!this.$['offline-gaia'].hidden)
        this.$['offline-gaia'].focus();
    }.bind(this), 100);
  },

  /**
   * Invoked when a frame emits 'dialogShown' event.
   * @private
   */
  onDialogShown_() {
    this.navigationEnabled_ = false;
  },

  /**
   * Invoked when a frame emits 'dialogHidden' event.
   * @private
   */
  onDialogHidden_() {
    this.navigationEnabled_ = true;
  },

  /**
   * Invoked when user activates menu item.
   * @param {!CustomEvent} e
   * @private
   */
  onMenuItemClicked_(e) {
    if (e.detail == 'ee') {
      cr.ui.Oobe.handleAccelerator(ACCELERATOR_ENROLLMENT);
    }
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
   * Invoked when a frame emits 'backButton' event.
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  onBackButton_(e) {
    this.getActiveFrame_().focus();
    this.lastBackMessageValue_ = !!e.detail;
    this.updateGuestButtonVisibility_();
  },
  /**
   * Invoked when the auth host emits 'setPrimaryActionEnabled'  event
   * @private
   */
  onSetPrimaryActionEnabled_(e) {
    this.primaryActionButtonEnabled_ = e.detail;
    this.maybeClickPrimaryActionButtonForTesting_();
  },

  /**
   * Invoked when the auth host emits 'setSecondaryActionEnabled'  event
   * @private
   */
  onSetSecondaryActionEnabled_(e) {
    this.secondaryActionButtonEnabled_ = e.detail;
  },

  /**
   * Invoked when the auth host emits 'setPrimaryActionLabel' event
   * @private
   */
  onSetPrimaryActionLabel_(e) {
    this.primaryActionButtonLabel_ = e.detail;
    this.maybeClickPrimaryActionButtonForTesting_();
  },

  /**
   * Invoked when the auth host emits 'setSecondaryActionLabel' event
   * @private
   */
  onSetSecondaryActionLabel_(e) {
    this.secondaryActionButtonLabel_ = e.detail;
  },

  /**
   * Invoked when the auth host emits 'setAllActionsEnabled' event
   * @private
   */
  onSetAllActionsEnabled_(e) {
    this.onSetPrimaryActionEnabled_(e);
    this.onSetSecondaryActionEnabled_(e);
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
          loadTimeData.getString('fatalErrorMessageVerificationFailed'),
          loadTimeData.getString('fatalErrorTryAgainButton'));
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
        loadTimeData.getStringF('fatalErrorMessageInsecureURL', url),
        loadTimeData.getString('fatalErrorDoneButton'));
  },

  /**
   * Shows the fatal auth error.
   * @param {string} message The error message to show.
   * @param {string} buttonLabel The label to display on dismiss button.
   * @private
   */
  showFatalAuthError_(message, buttonLabel) {
    login.FatalErrorScreen.show(message, buttonLabel, Oobe.showSigninUI);
  },

  /**
   * Show fatal auth error when information is missing from GAIA.
   * @private
   */
  missingGaiaInfo_() {
    this.showFatalAuthError_(
        loadTimeData.getString('fatalErrorMessageNoAccountDetails'),
        loadTimeData.getString('fatalErrorTryAgainButton'));
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
    if (this.screenMode_ == AuthMode.AD_AUTH) {
      this.email_ = credentials.username;
      chrome.send(
          'completeAdAuthentication',
          [credentials.username, credentials.password]);
    } else if (credentials.publicSAML) {
      this.email_ = credentials.email;
      chrome.send('launchSAMLPublicSession', [credentials.email]);
    } else if (credentials.useOffline) {
      this.email_ = credentials.email;
      chrome.send(
          'completeOfflineAuthentication',
          [credentials.email, credentials.password]);
    } else {
      chrome.send('completeAuthentication', [
        credentials.gaiaId, credentials.email, credentials.password,
        credentials.usingSAML, credentials.services,
        credentials.passwordAttributes
      ]);
    }

    // Hide the back button and the border line as they are not useful when
    // the loading screen is shown.
    this.setBackNavigationVisibility_(false);

    // Clear any error messages that were shown before login.
    Oobe.clearErrors();

    this.clearVideoTimer_();
    this.authCompleted_ = true;
    this.updateGuestButtonVisibility_();
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
   * Invoked when identifierEntered message received.
   * @param {!CustomEvent<!Object>} e Event with payload containing:
   *     {string} accountIdentifier User identifier.
   * @private
   */
  onIdentifierEnteredMessage_(e) {
    this.onIdentifierEntered_(e.detail);
  },

  /**
   * Clears input fields and switches to input mode.
   * @param {boolean} takeFocus True to take focus.
   * @param {boolean} forceOnline Whether online sign-in should be forced.
   * If |forceOnline| is false previously used sign-in type will be used.
   */
  reset(takeFocus, forceOnline) {
    // Reload and show the sign-in UI if needed.
    this.authenticator_.resetStates();
    if (takeFocus) {
      if (!forceOnline && this.isOffline_()) {
        Oobe.getInstance().setOobeUIState(OOBE_UI_STATE.GAIA_SIGNIN);
        // Do nothing, since offline version is reloaded after an error comes.
      } else {
        Oobe.showSigninUI();
      }
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
    this.lastBackMessageValue_ = false;
    this.authCompleted_ = false;
    this.updateGuestButtonVisibility_();
  },

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attempts tried.
   * @param {HTMLElement} error Content to show in bubble.
   */
  showErrorBubble(loginAttempts, error) {
    if (this.isOffline_()) {
      // Reload offline version of the sign-in extension, which will show
      // error itself.
      chrome.send('offlineLogin', [this.email_]);
    } else if (!this.loadingFrameContents_) {
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
  cancel() {
    this.clearVideoTimer_();

    // TODO(crbug.com/470893): Figure out whether/which of these exit conditions
    // are useful.
    if (this.isAllowlistErrorShown_ || this.authCompleted_) {
      return;
    }

    if (this.screenMode_ == AuthMode.AD_AUTH)
      chrome.send('cancelAdAuthentication');

    this.userActed('cancel');
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
   * Sets enterprise info strings for offline gaia.
   * Also sets callback and sends message whether we already have email and
   * should switch to the password screen with error.
   * @private
   */
  loadOffline_(params) {
    this.loadingFrameContents_ = true;
    this.startLoadingTimer_();
    const offlineLogin = this.$['offline-gaia'];
    offlineLogin.reset();
    if ('enterpriseDisplayDomain' in params)
      offlineLogin.domain = params['enterpriseDisplayDomain'];
    if ('emailDomain' in params)
      offlineLogin.emailDomain = '@' + params['emailDomain'];
    offlineLogin.setEmail(params.email);
    this.onAuthReady_();
  },

  /** @private */
  loadAdAuth_(params) {
    this.loadingFrameContents_ = true;
    this.startLoadingTimer_();
    const adAuthUI = this.getActiveFrame_();
    adAuthUI.realm = params['realm'];

    if ('emailDomain' in params)
      adAuthUI.userRealm = '@' + params['emailDomain'];

    adAuthUI.userName = params['email'];
    adAuthUI.focus();
    this.onAuthReady_();
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

    this.updateGuestButtonVisibility_();
  },

  /**
   * Show/Hide back navigation during post-authentication.
   * @param {boolean} visible Show/hide back navigation.
   * @private
   */
  setBackNavigationVisibility_(visible) {
    this.$['signin-back-button'].hidden = !visible;
    this.$['signin-frame-dialog'].setAttribute('hide-shadow', !visible);
    if (!visible) {
      // Also hide the primary and secondary action buttons
      this.primaryActionButtonLabel_ = null;
      this.secondaryActionButtonLabel_ = null;
    }
  },

  /**
   * @param {string} username
   * @param {ACTIVE_DIRECTORY_ERROR_STATE} errorState
   */
  invalidateAd(username, errorState) {
    if (this.screenMode_ != AuthMode.AD_AUTH)
      return;
    const adAuthUI = this.getActiveFrame_();
    adAuthUI.userName = username;
    adAuthUI.errorState = errorState;
    this.authCompleted_ = false;
    this.loadingFrameContents_ = false;
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
   * Checks if current step is one of specified steps.
   * @param {DialogMode} currentStep Name of current step.
   * @param {...string} stepsVarArgs List of steps to compare with.
   * @return {boolean}
   */
  isStep_(currentStep, ...stepsVarArgs) {
    if (stepsVarArgs.length < 1)
      throw Error('At least one step to compare is required.');
    return stepsVarArgs.some(step => currentStep === step);
  },

  /**
   * Updates current UI step based on internal state.
   * @param {number} mode
   * @param {OobeTypes.SecurityTokenPinDialogParameter} pinParams
   * @param {boolean} isLoading
   * @param {boolean} isAllowlistError
   * @private
   */
  refreshDialogStep_(mode, pinParams, isLoading, isAllowlistError) {
    if (pinParams !== null) {
      this.step_ = DialogMode.PIN_DIALOG;
      return;
    }
    if (isLoading) {
      if (mode == AuthMode.DEFAULT) {
        this.step_ = DialogMode.GAIA_LOADING;
      } else {
        this.step_ = DialogMode.LOADING;
      }
      return;
    }
    if (isAllowlistError) {
      this.step_ = DialogMode.GAIA_ALLOWLIST_ERROR;
      return;
    }
    switch (mode) {
      case AuthMode.DEFAULT:
        this.step_ = DialogMode.GAIA;
        break;
      case AuthMode.SAML_INTERSTITIAL:
        this.step_ = DialogMode.SAML_INTERSTITIAL;
        break;
      case AuthMode.OFFLINE:
        this.step_ = DialogMode.OFFLINE_GAIA;
        break;
      case AuthMode.AD_AUTH:
        this.step_ = DialogMode.OFFLINE_AD;
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

  /**
   * Whether popup overlay should be open.
   * @param {boolean} navigationEnabled
   * @param {boolean} isSamlSsoVisible
   * @return {boolean}
   */
  showOverlay_(navigationEnabled, isSamlSsoVisible) {
    return !navigationEnabled || isSamlSsoVisible;
  },

  clickPrimaryButtonForTesting() {
    this.clickPrimaryActionButtonForTesting_ = true;
    this.maybeClickPrimaryActionButtonForTesting_();
  },

  maybeClickPrimaryActionButtonForTesting_() {
    if (!this.clickPrimaryActionButtonForTesting_)
      return;

    const button = this.$['primary-action-button'];
    if (button.hidden || button.disabled)
      return;

    this.clickPrimaryActionButtonForTesting_ = false;
    button.click();
  },

  /**
   * Called when focus is returned.
   */
  onFocusReturned() {
    this.focusActiveFrame_();
  },
});
})();
