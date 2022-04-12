// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element to handle Gaia authentication. Encapsulates
 * authenticator.js and SAML notice handling.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const GaiaDialogBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior], Polymer.Element);

const CHROMEOS_GAIA_PASSWORD_METRIC = 'ChromeOS.Gaia.PasswordFlow';

/**
 * @polymer
 */
class GaiaDialog extends GaiaDialogBase {
  static get is() {
    return 'gaia-dialog';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
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
       * Type of bottom buttons.
       */
      gaiaDialogButtonsType: {
        type: String,
        value: OobeTypes.GaiaDialogButtonsType.DEFAULT,
      },

      /**
       * Whether the dialog can be closed.
       */
      isClosable: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether SAML IdP page is shown
       */
      isSamlSsoVisible: {
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
      authDomain: {
        type: String,
        value: '',
        notify: true,
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

      /* Defines name of the webview. Useful for tests. To find Guestview for
       * the JSChecker.
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

      /**
       * Whether the redirect to default IdP without interstitial step is
       * enabled.
       * @private {boolean}
       */
      flagRedirectToDefaultIdPEnabled_: {
        type: Boolean,
        value: loadTimeData.getBoolean('isRedirectToDefaultIdPEnabled'),
      },

      isSamlBackButtonHidden_: {
        type: Boolean,
        computed: 'isSamlBackButtonHidden(isDefaultSsoProvider, isClosable,' +
            'flagRedirectToDefaultIdPEnabled_)',
      }
    };
  }

  constructor() {
    super();
    /**
     * Emulate click on the primary action button when it is visible and
     * enabled.
     * @type {boolean}
     * @private
     */
    this.clickPrimaryActionButtonForTesting_ = false;
    /**
     * Emulate click on the primary action button when it is visible and
     * enabled.
     * @type {boolean}
     * @private
     */
    this.clickPrimaryActionButtonForTesting_ = false;
    /**
     * @type {!cr.login.Authenticator|undefined}
     * @private
     */
    this.authenticator_ = undefined;
  }

  getAuthenticator() {
    return this.authenticator_;
  }

  /** @override */
  ready() {
    super.ready();
    const webview = /** @type {!WebView} */ (this.$['signin-frame']);
    this.authenticator_ = new cr.login.Authenticator(webview);
    /**
     * Event listeners for the events triggered by the authenticator.
     */
    const authenticatorEventListeners = {
      // Note for the lowercase of fired events.
      'identifierEntered': (e) => {
        this.dispatchEvent(new CustomEvent(
            'identifierentered',
            {bubbles: true, composed: true, detail: e.detail}));
      },
      'loadAbort': (e) => {
        this.dispatchEvent(new CustomEvent(
            'webviewerror', {bubbles: true, composed: true, detail: e.detail}));
      },
      'ready': (e) => {
        this.dispatchEvent(new CustomEvent(
            'ready', {bubbles: true, composed: true, detail: e.detail}));
      },
      'showView': (e) => {
        this.dispatchEvent(new CustomEvent(
            'showview', {bubbles: true, composed: true, detail: e.detail}));
      },
      'menuItemClicked': (e) => {
        if (e.detail == 'ee') {
          this.dispatchEvent(new CustomEvent(
              'startenrollment', {bubbles: true, composed: true}));
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
        this.authDomain = e.newValue;
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
        this.dispatchEvent(new CustomEvent(
            'exit', {bubbles: true, composed: true, detail: e.detail}));
      },
      'removeUserByEmail': (e) => {
        this.dispatchEvent(new CustomEvent(
            'removeuserbyemail',
            {bubbles: true, composed: true, detail: e.detail}));
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
        this.dispatchEvent(new CustomEvent(
            'authcompleted',
            {bubbles: true, composed: true, detail: e.detail}));
      },
    };

    for (let eventName in authenticatorEventListeners) {
      this.authenticator_.addEventListener(
          eventName, authenticatorEventListeners[eventName].bind(this));
    }

    cr.sendWithPromise('getIsSshConfigured')
        .then(this.updateSshWarningVisibility.bind(this));
  }

  updateSshWarningVisibility(show) {
    this.$.sshWarning.hidden = !show;
  }

  show() {
    this.navigationEnabled = true;
    chrome.send('enableShelfButtons', [true]);
    this.getFrame().focus();
  }

  getFrame() {
    // Note: Can't use |this.$|, since it returns cached references to elements
    // originally present in DOM, while the signin-frame is  dynamically
    // recreated (see Authenticator.setWebviewPartition()).
    return this.shadowRoot.querySelector('#signin-frame');
  }

  clickPrimaryButtonForTesting() {
    this.clickPrimaryActionButtonForTesting_ = true;
    this.maybeClickPrimaryActionButtonForTesting_();
  }

  maybeClickPrimaryActionButtonForTesting_() {
    if (!this.clickPrimaryActionButtonForTesting_)
      return;

    const button = this.$['primary-action-button'];
    if (button.hidden || button.disabled)
      return;

    this.clickPrimaryActionButtonForTesting_ = false;
    button.click();
  }

  /* @private */
  getSamlNoticeMessage_(locale, videoEnabled, authDomain) {
    if (videoEnabled) {
      return this.i18n('samlNoticeWithVideo', authDomain);
    }
    return this.i18n('samlNotice', authDomain);
  }

  /* @private */
  close_() {
    this.dispatchEvent(
        new CustomEvent('closesaml', {bubbles: true, composed: true}));
  }

  /* @private */
  onChangeSigninProviderClicked_() {
    this.dispatchEvent(new CustomEvent(
        'changesigninprovider', {bubbles: true, composed: true}));
  }

  /* @private */
  onBackButtonClicked_() {
    if (this.canGoBack) {
      this.getFrame().back();
      return;
    }
    this.dispatchEvent(
        new CustomEvent('backcancel', {bubbles: true, composed: true}));
  }

  /**
   * Handles clicks on "PrimaryAction" button.
   * @private
   */
  onPrimaryActionButtonClicked_() {
    this.authenticator_.sendMessageToWebview('primaryActionHit');
  }

  /**
   * Handles clicks on "SecondaryAction" button.
   * @private
   */
  onSecondaryActionButtonClicked_() {
    this.authenticator_.sendMessageToWebview('secondaryActionHit');
  }

  /**
   * Handles clicks on Kiosk enrollment button.
   * @private
   */
  onKioskButtonClicked_() {
    this.setLicenseType_(OobeTypes.LicenseType.KIOSK);
    this.onPrimaryActionButtonClicked_();
  }

  /**
   * Handles clicks on Kiosk enrollment button.
   * @private
   */
  onEnterpriseButtonClicked_() {
    this.setLicenseType_(OobeTypes.LicenseType.ENTERPRISE);
    this.onPrimaryActionButtonClicked_();
  }

  /**
   * @param {number} licenseType - license to use.
   * @private
   */
  setLicenseType_(licenseType) {
    this.dispatchEvent(new CustomEvent(
        'licensetypeselected',
        {bubbles: true, composed: true, detail: licenseType}));
  }

  /**
   * Whether the button is enabled.
   * @param {boolean} navigationEnabled - whether navigation in general is
   * enabled.
   * @param {boolean} buttonEnabled - whether a specific button is enabled.
   * @private
   */
  isButtonEnabled_(navigationEnabled, buttonEnabled) {
    return navigationEnabled && buttonEnabled;
  }

  /**
   * Whether the back button is hidden.
   * @param {boolean} hideBackButtonIfCantGoBack - whether it should be hidden.
   * @param {boolean} canGoBack - whether the form can go back.
   * @private
   */
  isBackButtonHidden(hideBackButtonIfCantGoBack, canGoBack) {
    return hideBackButtonIfCantGoBack && !canGoBack;
  }

  /**
   * Whether the back button on SAML screen is hidden.
   * @param {boolean} isDefaultSsoProvider - whether it is default SAML page.
   * @param {boolean} isClosable - whether the form can be closed.
   * @param {boolean} flagRedirectToDefaultIdPEnabled - whether redirect to
   *     default IdP is enabled.
   * @private
   */
  isSamlBackButtonHidden(
      isDefaultSsoProvider, isClosable, flagRedirectToDefaultIdPEnabled) {
    return !flagRedirectToDefaultIdPEnabled ||
        isDefaultSsoProvider && !isClosable;
  }

  /**
   * Whether popup overlay should be open.
   * @param {boolean} navigationEnabled
   * @param {boolean} isSamlSsoVisible
   * @return {boolean}
   */
  showOverlay_(navigationEnabled, isSamlSsoVisible) {
    return !navigationEnabled || isSamlSsoVisible;
  }

  /**
   * Whether default navigation (original, as gaia has) is shown.
   * @param {boolean} canGoBack
   * @param {string} gaiaDialogButtonsType
   * @return {boolean}
   * @private
   */
  isDefaultNavigationShown_(canGoBack, gaiaDialogButtonsType) {
    return !canGoBack ||
        gaiaDialogButtonsType == OobeTypes.GaiaDialogButtonsType.DEFAULT;
  }

  /**
   * Whether Enterprise navigation is shown. Two buttons: primary for
   * Enterprise enrollment and secondary for Kiosk enrollment.
   * @param {boolean} canGoBack
   * @param {string} gaiaDialogButtonsType
   * @return {boolean}
   * @private
   */
  isEnterpriseNavigationShown_(canGoBack, gaiaDialogButtonsType) {
    return canGoBack &&
        gaiaDialogButtonsType ==
        OobeTypes.GaiaDialogButtonsType.ENTERPRISE_PREFERRED;
  }

  /**
   * Whether Kiosk navigation is shown. Two buttons: primary for
   * Kiosk enrollment and secondary for Enterprise enrollment.
   * @param {boolean} canGoBack
   * @param {string} gaiaDialogButtonsType
   * @return {boolean}
   * @private
   */
  isKioskNavigationShown_(canGoBack, gaiaDialogButtonsType) {
    return canGoBack &&
        gaiaDialogButtonsType ==
        OobeTypes.GaiaDialogButtonsType.KIOSK_PREFERRED;
  }
}

customElements.define(GaiaDialog.is, GaiaDialog);
