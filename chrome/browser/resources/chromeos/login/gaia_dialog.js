// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element to handle Gaia authentication. Encapsulates
 * authenticator.js and SAML notice handling.
 */

'use strict';

(function() {

const CHROMEOS_GAIA_PASSWORD_METRIC = 'ChromeOS.Gaia.PasswordFlow';

Polymer({
  is: 'gaia-dialog',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Whether SAML page uses camera.
     */
    videoEnabled: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /**
     * Current auth flow. See cr.login.Authenticator.AuthFlow
     */
    authFlow: {
      type: Number,
      value: 0,
      notify: true,
    },

    /**
     * Whether SAML IdP page is shown
     */
    isSamlSsoVisible: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether to hide back button if form can't go back.
     */
    hideBackButtonIfCantGoBack: {
      type: Boolean,
      value: false,
    },

    /**
     * Used to display SAML notice.
     * @private
     */
    authDomain_: {
      type: String,
      value: '',
    },

    /**
     * Controls navigation buttons enable state.
     */
    navigationEnabled: {
      type: Boolean,
      value: true,
      notify: true,
    },

    /**
     * Controls navigation buttons visibility.
     */
    navigationHidden: {
      type: Boolean,
      value: false,
    },

    /* Defines name of the webview. Useful for tests. To find Guestview for the
     * JSChecker.
     */
    webviewName: {
      type: String,
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
     * True if Gaia indicates that it can go back (e.g. on the password page)
     */
    canGoBack: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /**
     * Whether a pop-up overlay should be shown. This overlay is necessary
     * when GAIA shows an overlay within their iframe. It covers the parts
     * of the screen that would otherwise not show an overlay.
     * @private
     */
    isPopUpOverlayVisible_: {
      type: Boolean,
      computed: 'showOverlay_(navigationEnabled, isSamlSsoVisible)'
    },
  },

  /**
   * Emulate click on the primary action button when it is visible and enabled.
   * @type {boolean}
   * @private
   */
  clickPrimaryActionButtonForTesting_: false,

  /**
   * @type {!cr.login.Authenticator|undefined}
   * @private
   */
  authenticator_: undefined,

  getAuthenticator() {
    return this.authenticator_;
  },

  /** @override */
  ready() {
    const webview = this.$['signin-frame'];
    this.authenticator_ = new cr.login.Authenticator(webview);
    /**
     * Event listeners for the events triggered by the authenticator.
     */
    const authenticatorEventListeners = {
      // Note for the lowercase of fired events.
      'identifierEntered': (e) => {
        this.fire('identifierentered', e.detail);
      },
      'loadAbort': (e) => {
        this.fire('webviewerror', e.detail);
      },
      'ready': (e) => {
        this.fire('ready', e.detail);
      },
      'showView': (e) => {
        this.fire('showview', e.detail);
      },
      'menuItemClicked': (e) => {
        if (e.detail == 'ee') {
          this.fire('startenrollment');
        }
      },
      'backButton': (e) => {
        this.canGoBack = !!e.detail;
        this.getFrame().focus();
      },

      'setPrimaryActionEnabled': (e) => {
        this.primaryActionButtonEnabled_ = e.detail;
        this.maybeClickPrimaryActionButtonForTesting_();
      },
      'setPrimaryActionLabel': (e) => {
        this.primaryActionButtonLabel_ = e.detail;
        this.maybeClickPrimaryActionButtonForTesting_();
      },
      'setSecondaryActionEnabled': (e) => {
        this.secondaryActionButtonEnabled_ = e.detail;
      },
      'setSecondaryActionLabel': (e) => {
        this.secondaryActionButtonLabel_ = e.detail;
      },
      'setAllActionsEnabled': (e) => {
        this.primaryActionButtonEnabled_ = e.detail;
        this.secondaryActionButtonEnabled_ = e.detail;
        this.maybeClickPrimaryActionButtonForTesting_();
      },
      'videoEnabledChange': (e) => {
        this.videoEnabled = e.newValue;
      },
      'authFlowChange': (e) => {
        this.authFlow = e.newValue;
      },
      'authDomainChange': (e) => {
        this.authDomain_ = e.newValue;
      },
      'dialogShown': (e) => {
        this.navigationEnabled = false;
        chrome.send('enableShelfButtons', [false]);
      },
      'dialogHidden': (e) => {
        this.navigationEnabled = true;
        chrome.send('enableShelfButtons', [true]);
      },
      'exit': (e) => {
        this.fire('exit', e.detail);
      },
      'removeUserByEmail': (e) => {
        this.fire('removeuserbyemail', e.detail);
      },
      'apiPasswordAdded': (e) => {
        // Only record the metric for Gaia flow without 3rd-party SAML IdP.
        if (this.authFlow !== cr.login.Authenticator.AuthFlow.DEFAULT)
          return;
        chrome.send(
            'metricsHandler:recordBooleanHistogram',
            [CHROMEOS_GAIA_PASSWORD_METRIC, false]);
        chrome.send('passwordEntered');
      },
      'authCompleted': (e) => {
        // Only record the metric for Gaia flow without 3rd-party SAML IdP.
        if (this.authFlow === cr.login.Authenticator.AuthFlow.DEFAULT) {
          chrome.send(
              'metricsHandler:recordBooleanHistogram',
              [CHROMEOS_GAIA_PASSWORD_METRIC, true]);
        }
        this.fire('authcompleted', e.detail);
      },
    };

    for (var eventName in authenticatorEventListeners) {
      this.authenticator_.addEventListener(
          eventName, authenticatorEventListeners[eventName].bind(this));
    }

    cr.sendWithPromise('getIsSshConfigured')
        .then(this.updateSshWarningVisibility.bind(this));
  },

  updateSshWarningVisibility(show) {
    this.$.sshWarning.hidden = !show;
  },

  show() {
    this.navigationEnabled = true;
    chrome.send('enableShelfButtons', [true]);
    this.getFrame().focus();
  },

  getFrame() {
    // Note: Can't use |this.$|, since it returns cached references to elements
    // originally present in DOM, while the signin-frame is  dynamically
    // recreated (see Authenticator.setWebviewPartition()).
    return this.shadowRoot.querySelector('#signin-frame');
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

  /* @private */
  getSamlNoticeMessage_(locale, videoEnabled, authDomain) {
    if (videoEnabled) {
      return this.i18n('samlNoticeWithVideo', authDomain);
    }
    return this.i18n('samlNotice', authDomain);
  },

  /* @private */
  close_() {
    this.fire('closesaml');
  },

  /* @private */
  onBackButtonClicked_() {
    if (this.canGoBack) {
      this.getFrame().back();
      return;
    }
    this.fire('backcancel');
  },

  /**
   * Handles clicks on "PrimaryAction" button.
   * @private
   */
  onPrimaryActionButtonClicked_() {
    this.authenticator_.sendMessageToWebview('primaryActionHit');
  },

  /**
   * Handles clicks on "SecondaryAction" button.
   * @private
   */
  onSecondaryActionButtonClicked_() {
    this.authenticator_.sendMessageToWebview('secondaryActionHit');
  },

  /**
   * Whether the button is enabled.
   * @param {boolean} navigationEnabled - whether navigation in general is
   * enabled.
   * @param {boolean} buttonEnabled - whether a specific button is enabled.
   * @private
   */
  isButtonEnabled_(navigationEnabled, buttonEnabled) {
    return navigationEnabled && buttonEnabled;
  },

  /**
   * Whether the back button is hidden.
   * @param {boolean} hideBackButtonIfCantGoBack - whether it should be hidden.
   * @param {boolean} canGoBack - whether the form can go back.
   * @private
   */
  isBackButtonHidden(hideBackButtonIfCantGoBack, canGoBack) {
    return hideBackButtonIfCantGoBack && !canGoBack;
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

});
})();
