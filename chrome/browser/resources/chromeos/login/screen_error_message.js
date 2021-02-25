// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline message screen implementation.
 */

(function() {
var USER_ACTION_LAUNCH_OOBE_GUEST = 'launch-oobe-guest';
var USER_ACTION_LOCAL_STATE_POWERWASH = 'local-state-error-powerwash';
var USER_ACTION_SHOW_CAPTIVE_PORTAL = 'show-captive-portal';

// Array of the possible UI states of the screen. Must be in the
// same order as ErrorScreen::UIState enum values.
/** @const */ var UI_STATES = [
  ERROR_SCREEN_UI_STATE.UNKNOWN,
  ERROR_SCREEN_UI_STATE.UPDATE,
  ERROR_SCREEN_UI_STATE.SIGNIN,
  ERROR_SCREEN_UI_STATE.SUPERVISED_USER_CREATION_FLOW,
  ERROR_SCREEN_UI_STATE.KIOSK_MODE,
  ERROR_SCREEN_UI_STATE.LOCAL_STATE_ERROR,
  ERROR_SCREEN_UI_STATE.AUTO_ENROLLMENT_ERROR,
  ERROR_SCREEN_UI_STATE.ROLLBACK_ERROR,
];

// The help topic linked from the auto enrollment error message.
/** @const */ var HELP_TOPIC_AUTO_ENROLLMENT = 4632009;

// Possible error states of the screen.
/** @const */ var ERROR_STATE = {
  UNKNOWN: 'unknown',
  PORTAL: 'portal',
  OFFLINE: 'offline',
  PROXY: 'proxy',
  AUTH_EXT_TIMEOUT: 'auth-ext-timeout',
  KIOSK_ONLINE: 'kiosk-online',
  NONE: '',
};

// Possible error states of the screen. Must be in the same order as
// ErrorScreen::ErrorState enum values.
/** @const */ var ERROR_STATES = [
  ERROR_STATE.UNKNOWN,
  ERROR_STATE.PORTAL,
  ERROR_STATE.OFFLINE,
  ERROR_STATE.PROXY,
  ERROR_STATE.AUTH_EXT_TIMEOUT,
  ERROR_STATE.NONE,
  ERROR_STATE.KIOSK_ONLINE,
];

Polymer({
  is: 'error-message-element',

  behaviors: [
    OobeI18nBehavior,
    OobeDialogHostBehavior,
    LoginScreenBehavior,
  ],

  EXTERNAL_API: [
    'allowGuestSignin',
    'allowOfflineLogin',
    'setUIState',
    'setErrorState',
    'showConnectingIndicator',
    'setErrorStateNetwork',
    'setIsPersistentError',
  ],

  properties: {
    /**
     * Error screen initial UI state.
     * @private
     */
    uiState_: {
      type: String,
      value: ERROR_SCREEN_UI_STATE.UNKNOWN,
      observer: 'updateLocalizedContent',
    },

    /**
     * Error screen initial error state.
     * @private
     */
    errorState_: {
      type: String,
      value: ERROR_STATE.UNKNOWN,
      observer: 'updateLocalizedContent',
    },

    /**
     * True if it is forbidden to close the error message.
     * @private
     */
    is_persistent_error_: {
      type: Boolean,
      value: false,
    },

    /**
     * Controls if periodic background Wi-Fi scans are enabled to update the
     * list of available networks.
     * @private
     */
    enableWifiScans_: {
      type: Boolean,
      value: false,
    },

    currentNetworkName_: {
      type: String,
      value: '',
      observer: 'updateLocalizedContent',
    },

    /**
     * True if guest signin is allowed from the error screen.
     * @private
     */
    guestSessionAllowed_: {
      type: Boolean,
      value: false,
      observer: 'updateLocalizedContent',
    },

    /**
     * True if offline login is allowed from the error screen.
     * @private
     */
    offlineLoginAllowed_: {
      type: Boolean,
      value: false,
      observer: 'updateLocalizedContent',
    },

    /**
     * True if connecting indicator is shown.
     * @private
     */
    connectingIndicatorShown_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * @suppress {checkTypes} isOneOf_ allows arbitrary number of arguments.
   */
  getDialogTitle_() {
    if (this.isOneOf_(this.errorState_, 'portal', 'offline')) {
      return this.i18n('captivePortalTitle');
    } else if (
        this.isOneOf_(this.uiState_, 'ui-state-local-state-error') ||
        this.isOneOf_(this.errorState_, 'proxy', 'auth-ext-timeout')) {
      return this.i18n('loginErrorTitle');
    } else if (this.isOneOf_(this.errorState_, 'kiosk-online')) {
      return this.i18n('kioskOnlineTitle');
    } else if (this.isOneOf_(this.uiState_, 'ui-state-rollback-error')) {
      return this.i18n('rollbackErrorTitle');
    } else {
      return '';
    }
  },

  /**
   * Whether the screen can be closed.
   * |is_persistent_error_| prevents error screen to be closable even
   * if there are some user pods.
   * (E.g. out of OOBE process on the sign-in screen).
   * @type {boolean}
   */
  get closable() {
    return Oobe.getInstance().hasUserPods && !this.is_persistent_error_;
  },

  /**
   * Returns default event target element.
   * @type {Object}
   */
  get defaultControl() {
    return this.$.dialog;
  },

  ready() {
    this.initializeLoginScreen('ErrorMessageScreen', {
      resetAllowed: true,
    });

    this.updateLocalizedContent();
  },


  /**
   * Checks if the state ( === arguments[0]) is equal to one of the following
   * arguments.
   * @param {!string} state State name.
   */
  isOneOf_(state) {
    return Array.from(arguments).slice(1).includes(state);
  },

  rebootButtonClicked() {
    this.userActed('reboot');
  },

  diagnoseButtonClicked() {
    this.userActed('diagnose');
  },

  configureCertsButtonClicked() {
    this.userActed('configure-certs');
  },

  continueButtonClicked() {
    chrome.send('continueAppLaunch');
  },

  okButtonClicked() {
    this.userActed('cancel-reset');
  },

  powerwashButtonClicked() {
    this.userActed(USER_ACTION_LOCAL_STATE_POWERWASH);
  },

  onNetworkConnected_() {
    this.userActed('network-connected');
  },

  /**
   * Inserts translated `string_id` into `element_id` with substitutions and
   * anchor tag styles.
   * @param {string} element_id
   * @param {string} string_id
   * @param {SanitizeInnerHtmlOpts=} opts
   * @param  {Array<string>|string} anchor_ids
   */
  updateElementWithStringAndAnchorTag_(
      element_id, string_id, opts, ...anchor_ids) {
    opts = opts || {};
    opts.tags = opts.tags || [];
    opts.attrs = opts.attrs || [];
    opts.attrs = opts.attrs.concat(['id', 'class', 'is']);
    opts.substitutions = opts.substitutions || [];
    for (const anchorId of anchor_ids) {
      let attributes =
          ' class="oobe-local-link focus-on-show" is="action-link"';
      opts.substitutions = opts.substitutions.concat(
          ['<a id="' + anchorId + '"' + attributes + '>', '</a>']);
    }
    this.shadowRoot.getElementById(element_id).innerHTML =
        this.i18nAdvanced(string_id, opts);
    // oobe-dialog focuses first element with focus-on-show class that is not
    // hidden. We want to focus the first visible link. So we check if all
    // parent elements are visible, otherwise explicitly hide link, so it
    // won't be shown.
    let element = this.shadowRoot.getElementById(element_id);
    let hidden = false;
    while (element) {
      if (element.hidden) {
        hidden = true;
        break;
      }
      element = element.parentElement;
    }
    for (const anchorId of anchor_ids) {
      /** @suppress {checkTypes} anchorId is a string */
      let linkElement = this.shadowRoot.getElementById(anchorId);
      if (hidden) {
        linkElement.setAttribute('hidden', '');
      } else {
        linkElement.removeAttribute('hidden');
      }
    }
  },

  /**
   * Updates localized content of the screen that is not updated via template.
   */
  updateLocalizedContent() {
    var self = this;
    this.updateElementWithStringAndAnchorTag_(
        'auto-enrollment-offline-message-text',
        'autoEnrollmentOfflineMessageBody', {
          substitutions: [
            loadTimeData.getString('deviceType'),
            '<b>' + this.currentNetworkName_ + '</b>'
          ]
        },
        'auto-enrollment-learn-more');
    this.$$('#auto-enrollment-learn-more').onclick = function() {
      chrome.send('launchHelpApp', [HELP_TOPIC_AUTO_ENROLLMENT]);
    };

    this.updateElementWithStringAndAnchorTag_(
        'captive-portal-message-text', 'captivePortalMessage',
        {substitutions: ['<b>' + this.currentNetworkName_ + '</b>']},
        'captive-portal-fix-link');
    this.$$('#captive-portal-fix-link').onclick = function() {
      self.userActed(USER_ACTION_SHOW_CAPTIVE_PORTAL);
    };

    this.updateElementWithStringAndAnchorTag_(
        'captive-portal-proxy-message-text', 'captivePortalProxyMessage', {},
        'proxy-settings-fix-link');
    this.$$('#proxy-settings-fix-link').onclick = function() {
      chrome.send('openInternetDetailDialog');
    };

    this.updateElementWithStringAndAnchorTag_(
        'update-proxy-message-text', 'updateProxyMessageText', {},
        'update-proxy-error-fix-proxy');
    this.$$('#update-proxy-error-fix-proxy').onclick = function() {
      chrome.send('openInternetDetailDialog');
    };

    this.updateElementWithStringAndAnchorTag_(
        'signin-proxy-message-text', 'signinProxyMessageText', {},
        'proxy-error-signin-retry-link', 'signin-proxy-error-fix-proxy');
    this.$$('#proxy-error-signin-retry-link').onclick = function() {
      self.userActed('reload-gaia');
    };
    this.$$('#signin-proxy-error-fix-proxy').onclick = function() {
      chrome.send('openInternetDetailDialog');
    };

    this.updateElementWithStringAndAnchorTag_(
        'error-guest-signin', 'guestSignin', {}, 'error-guest-signin-link');
    this.$$('#error-guest-signin-link')
        .addEventListener('click', this.launchGuestSession_.bind(this));

    this.updateElementWithStringAndAnchorTag_(
        'error-guest-signin-fix-network', 'guestSigninFixNetwork', {},
        'error-guest-fix-network-signin-link');
    this.$$('#error-guest-fix-network-signin-link')
        .addEventListener('click', this.launchGuestSession_.bind(this));

    this.updateElementWithStringAndAnchorTag_(
        'error-offline-login', 'offlineLogin', {}, 'error-offline-login-link');
    this.$$('#error-offline-login-link').onclick = function() {
      chrome.send('offlineLogin');
    };
  },

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ERROR;
  },

  /**
   * Event handler that is invoked just before the screen is shown.
   * @param {Object} data Screen init payload.
   * @suppress {missingProperties} clearErrors() exists
   */
  onBeforeShow(data) {
    this.enableWifiScans_ = true;
    cr.ui.Oobe.clearErrors();
    this.$['backButton'].disabled = !this.closable;
  },

  /**
   * Event handler that is invoked just before the screen is hidden.
   * @suppress {missingProperties} setOobeUIState() exists
   */
  onBeforeHide() {
    this.enableWifiScans_ = false;
    Oobe.getInstance().setOobeUIState(OOBE_UI_STATE.HIDDEN);
    // Reset property to the default state.
    this.setIsPersistentError(false);
  },

  /**
   * Event handler for guest session launch.
   * @private
   * @suppress {missingProperties} isOobeUI() exists
   */
  launchGuestSession_() {
    if (Oobe.getInstance().isOobeUI()) {
      this.userActed(USER_ACTION_LAUNCH_OOBE_GUEST);
    } else {
      chrome.send('launchIncognito');
    }
  },

  /**
   * Prepares error screen to show guest signin link.
   * @private
   */
  allowGuestSignin(allowed) {
    this.guestSessionAllowed_ = allowed;
  },

  /**
   * Prepares error screen to show offline login link.
   * @private
   */
  allowOfflineLogin(allowed) {
    this.offlineLoginAllowed_ = allowed;
  },

  /**
   * Sets current UI state of the screen.
   * @param {number} ui_state New UI state of the screen.
   * @private
   */
  setUIState(ui_state) {
    this.uiState_ = UI_STATES[ui_state];
  },

  /**
   * Sets current error state of the screen.
   * @param {number} error_state New error state of the screen.
   * @private
   */
  setErrorState(error_state) {
    this.errorState_ = ERROR_STATES[error_state];
  },

  /**
   * Sets current error network state of the screen.
   * @param {string} network Name of the current network
   */
  setErrorStateNetwork(network) {
    this.currentNetworkName_ = network;
  },

  /**
   * Updates visibility of the label indicating we're reconnecting.
   * @param {boolean} show Whether the label should be shown.
   */
  showConnectingIndicator(show) {
    this.connectingIndicatorShown_ = show;
  },

  /**
   * Cancels error screen and drops to user pods.
   */
  cancel() {
    if (this.closable)
      this.userActed('cancel');
  },

  /**
   * Makes error message non-closable.
   */
  setIsPersistentError(is_persistent) {
    this.is_persistent_error_ = is_persistent;
  },
});
})();
