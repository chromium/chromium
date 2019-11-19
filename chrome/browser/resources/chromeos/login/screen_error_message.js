// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline message screen implementation.
 */

login.createScreen('ErrorMessageScreen', 'error-message', function() {
  var USER_ACTION_CONFIGURE_CERTS = 'configure-certs';
  var USER_ACTION_DIAGNOSE = 'diagnose';
  var USER_ACTION_LAUNCH_OOBE_GUEST = 'launch-oobe-guest';
  var USER_ACTION_LOCAL_STATE_POWERWASH = 'local-state-error-powerwash';
  var USER_ACTION_REBOOT = 'reboot';
  var USER_ACTION_SHOW_CAPTIVE_PORTAL = 'show-captive-portal';
  var USER_ACTION_NETWORK_CONNECTED = 'network-connected';

  // Link which starts guest session for captive portal fixing.
  /** @const */ var FIX_CAPTIVE_PORTAL_ID = 'captive-portal-fix-link';

  /** @const */ var FIX_PROXY_SETTINGS_ID = 'proxy-settings-fix-link';

  // Class of the elements which hold current network name.
  /** @const */ var CURRENT_NETWORK_NAME_CLASS = 'portal-network-name';

  // Link which triggers frame reload.
  /** @const */ var RELOAD_PAGE_ID = 'proxy-error-signin-retry-link';

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
    UNKNOWN: 'error-state-unknown',
    PORTAL: 'error-state-portal',
    OFFLINE: 'error-state-offline',
    PROXY: 'error-state-proxy',
    AUTH_EXT_TIMEOUT: 'error-state-auth-ext-timeout',
    KIOSK_ONLINE: 'error-state-kiosk-online'
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

  return {
    EXTERNAL_API: [
      'updateLocalizedContent',
      'onBeforeShow',
      'onBeforeHide',
      'allowGuestSignin',
      'allowOfflineLogin',
      'setUIState',
      'setErrorState',
      'showConnectingIndicator',
      'setErrorStateNetwork',
      'setIsPersistentError',
    ],

    // Error screen initial UI state.
    ui_state_: ERROR_SCREEN_UI_STATE.UNKNOWN,

    // Error screen initial error state.
    error_state_: ERROR_STATE.UNKNOWN,

    // True if it is forbidden to close the error message.
    is_persistent_error_: false,

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
      return $('error-message-md');
    },

    /** @override */
    decorate: function() {
      this.updateLocalizedContent();

      var self = this;
      $('error-message-back-button')
          .addEventListener('click', this.cancel.bind(this));

      $('error-message-md-reboot-button').addEventListener('tap', function(e) {
        self.send(login.Screen.CALLBACK_USER_ACTED, USER_ACTION_REBOOT);
        e.stopPropagation();
      });
      $('error-message-md-diagnose-button')
          .addEventListener('tap', function(e) {
            self.send(login.Screen.CALLBACK_USER_ACTED, USER_ACTION_DIAGNOSE);
            e.stopPropagation();
          });
      $('error-message-md-configure-certs-button')
          .addEventListener('tap', function(e) {
            self.send(
                login.Screen.CALLBACK_USER_ACTED, USER_ACTION_CONFIGURE_CERTS);
            e.stopPropagation();
          });
      $('error-message-md-continue-button')
          .addEventListener('tap', function(e) {
            chrome.send('continueAppLaunch');
            e.stopPropagation();
          });
      $('error-message-md-ok-button').addEventListener('tap', function(e) {
        chrome.send('login.ResetScreen.userActed', ['cancel-reset']);
        e.stopPropagation();
      });
      $('error-message-md-powerwash-button')
          .addEventListener('tap', function(e) {
            self.send(
                login.Screen.CALLBACK_USER_ACTED,
                USER_ACTION_LOCAL_STATE_POWERWASH);
            e.stopPropagation();
          });
      $('offline-network-control')
          .addEventListener('selected-network-connected', function(e) {
            self.send(
                login.Screen.CALLBACK_USER_ACTED,
                USER_ACTION_NETWORK_CONNECTED);
          });
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
      var self = this;
      $('auto-enrollment-offline-message-text').innerHTML =
          loadTimeData.getStringF(
              'autoEnrollmentOfflineMessageBody',
              loadTimeData.getString('deviceType'),
              '<b class="' + CURRENT_NETWORK_NAME_CLASS + '"></b>',
              '<a id="auto-enrollment-learn-more" class="signin-link" ' +
                  '"href="#">',
              '</a>');
      $('auto-enrollment-learn-more').onclick = function() {
        chrome.send('launchHelpApp', [HELP_TOPIC_AUTO_ENROLLMENT]);
      };

      $('captive-portal-message-text').innerHTML = loadTimeData.getStringF(
          'captivePortalMessage',
          '<b class="' + CURRENT_NETWORK_NAME_CLASS + '"></b>',
          '<a id="' + FIX_CAPTIVE_PORTAL_ID + '" class="signin-link" href="#">',
          '</a>');
      $(FIX_CAPTIVE_PORTAL_ID).onclick = function() {
        self.send(
            login.Screen.CALLBACK_USER_ACTED, USER_ACTION_SHOW_CAPTIVE_PORTAL);
      };

      $('captive-portal-proxy-message-text').innerHTML =
          loadTimeData.getStringF(
              'captivePortalProxyMessage',
              '<a id="' + FIX_PROXY_SETTINGS_ID +
                  '" class="signin-link" href="#">',
              '</a>');
      $(FIX_PROXY_SETTINGS_ID).onclick = function() {
        chrome.send('openInternetDetailDialog');
      };
      $('update-proxy-message-text').innerHTML = loadTimeData.getStringF(
          'updateProxyMessageText',
          '<a id="update-proxy-error-fix-proxy" class="signin-link" href="#">',
          '</a>');
      $('update-proxy-error-fix-proxy').onclick = function() {
        chrome.send('openInternetDetailDialog');
      };
      $('signin-proxy-message-text').innerHTML = loadTimeData.getStringF(
          'signinProxyMessageText',
          '<a id="' + RELOAD_PAGE_ID + '" class="signin-link" href="#">',
          '</a>',
          '<a id="signin-proxy-error-fix-proxy" class="signin-link" href="#">',
          '</a>');
      $(RELOAD_PAGE_ID).onclick = function() {
        var gaiaScreen = $(SCREEN_GAIA_SIGNIN);
        // Schedules an immediate retry.
        gaiaScreen.doReload();
      };
      $('signin-proxy-error-fix-proxy').onclick = function() {
        chrome.send('openInternetDetailDialog');
      };

      $('error-guest-signin').innerHTML = loadTimeData.getStringF(
          'guestSignin',
          '<a id="error-guest-signin-link" class="signin-link" href="#">',
          '</a>');
      $('error-guest-signin-link')
          .addEventListener('click', this.launchGuestSession_.bind(this));

      $('error-guest-signin-fix-network').innerHTML = loadTimeData.getStringF(
          'guestSigninFixNetwork',
          '<a id="error-guest-fix-network-signin-link" class="signin-link" ' +
              'href="#">',
          '</a>');
      $('error-guest-fix-network-signin-link')
          .addEventListener('click', this.launchGuestSession_.bind(this));

      $('error-offline-login').innerHTML = loadTimeData.getStringF(
          'offlineLogin',
          '<a id="error-offline-login-link" class="signin-link" href="#">',
          '</a>');
      $('error-offline-login-link').onclick = function() {
        chrome.send('offlineLogin');
      };

      var ellipsis = '';
      for (var i = 1; i <= 3; ++i) {
        ellipsis +=
            '<span id="connecting-indicator-ellipsis-' + i + '"></span>';
      }
      $('connecting-indicator').innerHTML =
          loadTimeData.getStringF('connectingIndicatorText', ellipsis);

      $('offline-network-control').setOncStrings();

      this.onContentChange_();
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {Object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      cr.ui.Oobe.clearErrors();
      Oobe.getInstance().setSigninUIState(SIGNIN_UI_STATE.ERROR);
      $('error-message-back-button').disabled = !this.closable;
    },

    /**
     * Event handler that is invoked just before the screen is hidden.
     */
    onBeforeHide: function() {
      Oobe.getInstance().setSigninUIState(SIGNIN_UI_STATE.HIDDEN);
      // Reset property to the default state.
      this.setIsPersistentError(false);
    },

    /**
     * Sets current UI state of the screen.
     * @param {string} ui_state New UI state of the screen.
     * @private
     */
    setUIState_: function(ui_state) {
      this.classList.remove(this.ui_state);
      this.ui_state = ui_state;
      this.classList.add(this.ui_state);
      this.onContentChange_();
    },

    /**
     * Sets current error state of the screen.
     * @param {string} error_state New error state of the screen.
     * @private
     */
    setErrorState_: function(error_state) {
      this.classList.remove(this.error_state);
      this.error_state = error_state;
      this.classList.add(this.error_state);
      this.onContentChange_();
    },

    /**
     * Sets network.
     * @param {string} network Name of the current network
     * @private
     */
    setNetwork_: function(network) {
      var networkNameElems =
          document.getElementsByClassName(CURRENT_NETWORK_NAME_CLASS);
      for (var i = 0; i < networkNameElems.length; ++i)
        networkNameElems[i].textContent = network;
      this.onContentChange_();
    },

    /* Method called after content of the screen changed.
     * @private
     */
    onContentChange_: function() {
      if (Oobe.getInstance().currentScreen === this) {
        Oobe.getInstance().updateScreenSize(this);
      }
    },

    /**
     * Event handler for guest session launch.
     * @private
     */
    launchGuestSession_: function() {
      if (Oobe.getInstance().isOobeUI()) {
        this.send(
            login.Screen.CALLBACK_USER_ACTED, USER_ACTION_LAUNCH_OOBE_GUEST);
      } else {
        chrome.send('launchIncognito');
      }
    },

    /**
     * Prepares error screen to show guest signin link.
     * @private
     */
    allowGuestSignin: function(allowed) {
      this.classList.toggle('allow-guest-signin', allowed);
      this.onContentChange_();
    },

    /**
     * Prepares error screen to show offline login link.
     * @private
     */
    allowOfflineLogin: function(allowed) {
      this.classList.toggle('allow-offline-login', allowed);
      this.onContentChange_();
    },

    /**
     * Sets current UI state of the screen.
     * @param {number} ui_state New UI state of the screen.
     * @private
     */
    setUIState: function(ui_state) {
      this.setUIState_(UI_STATES[ui_state]);
    },

    /**
     * Sets current error state of the screen.
     * @param {number} error_state New error state of the screen.
     * @private
     */
    setErrorState: function(error_state) {
      this.setErrorState_(ERROR_STATES[error_state]);
    },

    /**
     * Sets current error network state of the screen.
     * @param {string} network Name of the current network
     */
    setErrorStateNetwork: function(value) {
      this.setNetwork_(value);
    },

    /**
     * Updates visibility of the label indicating we're reconnecting.
     * @param {boolean} show Whether the label should be shown.
     */
    showConnectingIndicator: function(show) {
      this.classList.toggle('show-connecting-indicator', show);
      this.onContentChange_();
    },

    /**
     * Cancels error screen and drops to user pods.
     */
    cancel: function() {
      if (this.closable)
        Oobe.showUserPods();
    },

    /**
     * Makes error message non-closable.
     */
    setIsPersistentError: function(is_persistent) {
      this.is_persistent_error_ = is_persistent;
    }
  };
});
