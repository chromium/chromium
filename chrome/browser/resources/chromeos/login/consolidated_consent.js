// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview consolidated consent screen implementation.
 */
'use strict';

(function() {

// Enum that describes the current state of the Consolidated Consent screen
const UIState = {
  LOADING: 'loading',
  LOADED: 'loaded',
  ERROR: 'error',
  GOOGLE_EULA: 'google-eula',
  CROS_EULA: 'cros-eula',
  ARC: 'arc',
  PRIVACY: 'privacy',
};

/**
 * URL to use when online page is not available.
 * @type {string}
 */
const GOOGLE_EULA_TERMS_URL = 'chrome://terms';
const ARC_TERMS_URL = 'chrome://terms/arc/terms';
const PRIVACY_POLICY_URL = 'chrome://terms/arc/privacy_policy';

Polymer({
  is: 'consolidated-consent-element',

  behaviors: [OobeI18nBehavior, MultiStepBehavior, LoginScreenBehavior],

  EXTERNAL_API: [
    'SetUsageMode',
    'setBackupMode',
    'setLocationMode',
  ],

  properties: {
    isArcEnabled_: {
      type: Boolean,
      value: true,
    },

    isDemo_: {
      type: Boolean,
      value: false,
    },

    isChildAccount_: {
      type: Boolean,
      value: false,
    },

    usageManaged_: {
      type: Boolean,
      value: false,
    },

    backupManaged_: {
      type: Boolean,
      value: false,
    },

    locationManaged_: {
      type: Boolean,
      value: false,
    },

    usageChecked: {
      type: Boolean,
      value: true,
    },

    backupChecked: {
      type: Boolean,
      value: true,
    },

    locationChecked: {
      type: Boolean,
      value: true,
    },

    googleEulaLoading_: {
      type: Boolean,
      value: true,
    },

    crosEulaLoading_: {
      type: Boolean,
      value: true,
    },

    arcTosLoading_: {
      type: Boolean,
      value: true,
    },

    privacyPolicyLoading_: {
      type: Boolean,
      value: true,
    },
  },

  /**
   * Text displayed in the Arc Terms of Service webview.
   */
  arcTosContent_: '',

  /**
   * If online ARC ToS failed to load in the demo mode, the offline version
   * is loaded and `isArcTosUsingOfflineTerms_` is set to true.
   * @private {boolean}
   */
  isArcTosUsingOfflineTerms_: false,

  isArcTosInitialized_: false,

  /**
   * Flag that ensures that OOBE configuration is applied only once.
   * @private {boolean}
   */
  configuration_applied_: false,

  /**
   * The hostname of the url where the terms of service will be fetched.
   * Overwritten by tests to load terms of service from local test server.
   */
  termsOfServiceHostName_: 'https://play.google.com',

  /**
   * Online URLs
   */
  googleEulaUrl_: '',
  crosEulaUrl_: '',
  arcTosUrl_: '',

  defaultUIStep() {
    return UIState.LOADING;
  },

  UI_STEPS: UIState,

  ready() {
    this.initializeLoginScreen('ConsolidatedConsentScreen', {
      resetAllowed: true,
    });
    this.updateLocalizedContent();
  },

  onBeforeShow(data) {
    window.setTimeout(this.applyOobeConfiguration_);

    this.isArcEnabled_ = data['isArcEnabled'];
    this.isDemo_ = data['isDemo'];
    this.isChildAccount_ = data['isChildAccount'];

    this.googleEulaLoading_ = true;
    this.crosEulaLoading_ = true;
    this.arcTosLoading_ = true;

    this.googleEulaUrl_ = data['googleEulaUrl'];
    this.crosEulaUrl_ = data['crosEulaUrl'];
    this.arcTosUrl_ = this.termsOfServiceHostName_ + '/about/play-terms.html';

    const countryCode = data['countryCode'];
    this.initializeArcTos_(countryCode);
    this.loadWebviews_();

    if (this.isArcOptInsHidden_(this.isArcEnabled_, this.isDemo_)) {
      this.$.loadedContent.classList = 'landscape-header-aligned';
    } else {
      this.$.loadedContent.classList = 'landscape-vertical-centered';
    }
  },

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  },

  applyOobeConfiguration_: () => {
    if (this.configuration_applied_)
      return;

    var configuration = Oobe.getInstance().getOobeConfiguration();
    if (!configuration)
      return;

    if (configuration.eulaSendStatistics)
      this.usageChecked = true;

    if (configuration.eulaAutoAccept && configuration.arcTosAutoAccept)
      this.onAcceptClick_();
  },

  initializeArcTos_(countryCode) {
    if (this.isArcTosInitialized_)
      return;

    this.isArcTosInitialized_ = true;
    const webview = this.$.arcTosWebview;
    webview.removeContentScripts(['preProcess']);

    var language = this.getCurrentLanguage_();
    countryCode = countryCode.toLowerCase();

    var scriptSetParameters = 'document.countryCode = \'' + countryCode + '\';';
    scriptSetParameters += 'document.language = \'' + language + '\';';
    scriptSetParameters += 'document.viewMode = \'large-view\';';

    webview.addContentScripts([{
      name: 'preProcess',
      matches: [this.getTermsOfServiceHostNameForMatchPattern_() + '/*'],
      js: {code: scriptSetParameters},
      run_at: 'document_start'
    }]);

    this.$.arcTosOverlayWebview.addContentScripts([{
      name: 'postProcess',
      matches: ['https://support.google.com/*'],
      css: {files: ['overlay.css']},
      run_at: 'document_end'
    }]);

    webview.addEventListener('newwindow', (event) => {
      event.preventDefault();
      this.showArcTosOverlay(event.targetUrl);
    });
  },

  loadWebviews_() {
    this.loadEulaWebview_(
        this.$.googleEulaWebview, this.googleEulaUrl_,
        false /* clear_anchors */);
    this.loadEulaWebview_(
        this.$.crosEulaWebview, this.crosEulaUrl_, true /* clear_anchors */);
    this.loadArcTosWebview_(this.arcTosUrl_);
  },

  loadEulaWebview_(webview, online_tos_url, clear_anchors) {
    const loadFailureCallback = () => {
      WebViewHelper.loadUrlContentToWebView(
          webview, EULA_TERMS_URL, WebViewHelper.ContentType.HTML);
    };

    const tosLoader = new WebViewLoader(
        webview, loadFailureCallback, clear_anchors, true /* inject_css */);
    tosLoader.setUrl(online_tos_url);
  },

  loadArcTosWebview_(online_tos_url) {
    const webview = this.$.arcTosWebview;

    var loadFailureCallback = () => {
      if (this.isDemo_) {
        this.isArcTosUsingOfflineTerms_ = true;
        WebViewHelper.loadUrlContentToWebView(
            webview, ARC_TERMS_URL, WebViewHelper.ContentType.HTML);
        return;
      }
      this.setUIStep(UIState.ERROR);
    };

    var tosLoader = new WebViewLoader(
        webview, loadFailureCallback, false /* clear_anchors */,
        false /* inject_css */);
    tosLoader.setUrl(online_tos_url);
  },

  /**
   * Returns a match pattern compatible version of termsOfServiceHostName_ by
   * stripping the port number part of the hostname. During tests
   * termsOfServiceHostName_ will contain a port number part.
   * @return {string}
   * @private
   */
  getTermsOfServiceHostNameForMatchPattern_() {
    return this.termsOfServiceHostName_.replace(/:[0-9]+/, '');
  },

  /**
   * Returns current language that can be updated in OOBE flow. If OOBE flow
   * does not exist then use navigator.language.
   *
   * @private
   */
  getCurrentLanguage_() {
    const LANGUAGE_LIST_ID = 'languageList';
    if (loadTimeData.valueExists(LANGUAGE_LIST_ID)) {
      var languageList = /** @type {!Array<OobeTypes.LanguageDsc>} */ (
          loadTimeData.getValue(LANGUAGE_LIST_ID));
      if (languageList) {
        var language = getSelectedValue(languageList);
        if (language) {
          return language;
        }
      }
    }
    return navigator.language;
  },

  loadPrivacyPolicyWebview_(online_tos_url) {
    const webview = this.$.privacyPolicyWebview;

    var loadFailureCallback = () => {
      if (this.isDemo_) {
        WebViewHelper.loadUrlContentToWebView(
            webview, PRIVACY_POLICY_URL, WebViewHelper.ContentType.PDF);
      }
    };

    var tosLoader = new WebViewLoader(
        webview, loadFailureCallback, false /* clear_anchors */,
        false /* inject_css */);
    tosLoader.setUrl(online_tos_url);
  },

  onGoogleEulaContentLoad_() {
    this.googleEulaLoading_ = false;
    this.maybeSetLoadedStep_();
  },

  onCrosEulaContentLoad_() {
    this.crosEulaLoading_ = false;
  },

  maybeSetLoadedStep_() {
    if (!this.googleEulaLoading_ && !this.arcTosLoading_ &&
        this.uiStep == UIState.LOADING) {
      this.setUIStep(UIState.LOADED);
      this.$.acceptButton.focus();
    }
  },

  onArcTosContentLoad_() {
    const webview = this.$.arcTosWebview;

    if (this.isArcTosUsingOfflineTerms_) {
      // Process offline ToS. Scripts added to web view by addContentScripts()
      // are not executed when using data url.
      var setParameters =
          `document.body.classList.add('large-view', 'offline-terms');`;
      webview.executeScript({code: setParameters});
      webview.insertCSS({file: 'playstore.css'});

      // Load the offline terms for privacy policy
      WebViewHelper.loadUrlContentToWebView(
          webview, PRIVACY_POLICY_URL, WebViewHelper.ContentType.PDF);
    } else {
      webview.executeScript({code: 'getPrivacyPolicyLink();'}, (results) => {
        if (results && results.length == 1 && typeof results[0] == 'string') {
          this.loadPrivacyPolicyWebview_(results[0]);
        } else {
          var defaultLink = 'https://www.google.com/intl/' +
              this.getCurrentLanguage_() + '/policies/privacy/';
          this.loadPrivacyPolicyWebview_(defaultLink);
        }
      });
    }

    // In demo mode, consents are not recorded, so no need to store the ToS
    // Content.
    if (!this.isDemo_) {
      // Process online ToS.
      var getToSContent = {code: 'getToSContent();'};
      webview.executeScript(getToSContent, (results) => {
        if (!results || results.length != 1 || typeof results[0] !== 'string')
          return;
        this.arcTosContent_ = results[0];
      });
    }

    this.arcTosLoading_ = false;
    this.maybeSetLoadedStep_();
  },

  onPrivacyPolicyContentLoad_() {
    this.privacyPolicyLoading_ = false;
  },

  updateLocalizedContent() {
    this.$$('#privacyPolicyLink')
        .addEventListener('click', () => this.onPrivacyPolicyLinkClick_());
    this.$$('#googleEulaLink')
        .addEventListener('click', () => this.onGoogleEulaLinkClick_());
    this.$$('#googleEulaLinkArcDisabled')
        .addEventListener('click', () => this.onGoogleEulaLinkClick_());
    this.$$('#crosEulaLink')
        .addEventListener('click', () => this.onCrosEulaLinkClick_());
    this.$$('#crosEulaLinkArcDisabled')
        .addEventListener('click', () => this.onCrosEulaLinkClick_());
    this.$$('#arcTosLink')
        .addEventListener('click', () => this.onArcTosLinkClick_());
  },

  getSubtitle_(locale) {
    const subtitle = document.createElement('div');
    subtitle.innerHTML =
        this.i18nAdvanced('consolidatedConsentSubheader', {attrs: ['id']});

    const privacyPolicyLink = subtitle.querySelector('#privacyPolicyLink');
    privacyPolicyLink.setAttribute('is', 'action-link');
    privacyPolicyLink.classList.add('oobe-local-link');
    return subtitle.innerHTML;
  },

  getTermsDescription_(locale) {
    const description = document.createElement('div');
    description.innerHTML = this.i18nAdvanced(
        'consolidatedConsentTermsDescription', {attrs: ['id']});

    const googleEulaLink = description.querySelector('#googleEulaLink');
    googleEulaLink.setAttribute('is', 'action-link');
    googleEulaLink.classList.add('oobe-local-link');

    const crosEulaLink = description.querySelector('#crosEulaLink');
    crosEulaLink.setAttribute('is', 'action-link');
    crosEulaLink.classList.add('oobe-local-link');

    const arcTosLink = description.querySelector('#arcTosLink');
    arcTosLink.setAttribute('is', 'action-link');
    arcTosLink.classList.add('oobe-local-link');

    return description.innerHTML;
  },

  getTermsDescriptionArcDisabled_(locale) {
    const description = document.createElement('div');
    description.innerHTML = this.i18nAdvanced(
        'consolidatedConsentTermsDescriptionArcDisabled', {attrs: ['id']});

    const googleEulaLink =
        description.querySelector('#googleEulaLinkArcDisabled');
    googleEulaLink.setAttribute('is', 'action-link');
    googleEulaLink.classList.add('oobe-local-link');

    const crosEulaLink = description.querySelector('#crosEulaLinkArcDisabled');
    crosEulaLink.setAttribute('is', 'action-link');
    crosEulaLink.classList.add('oobe-local-link');

    return description.innerHTML;
  },

  getUsageText_(locale, isChildAccount, isArcEnabled, isDemo) {
    if (this.isArcOptInsHidden_(isArcEnabled, isDemo)) {
      return this.i18n('consolidatedConsentUsageOptInArcDisabled');
    }

    if (isChildAccount)
      return this.i18n('consolidatedConsentUsageOptInChild');
    return this.i18n('consolidatedConsentUsageOptIn');
  },

  getUsageLearnMoreText_(locale, isChildAccount, isArcEnabled, isDemo) {
    if (this.isArcOptInsHidden_(isArcEnabled, isDemo)) {
      if (isChildAccount) {
        return this.i18nAdvanced(
            'consolidatedConsentUsageOptInLearnMoreArcDisabledChild');
      }
      return this.i18nAdvanced(
          'consolidatedConsentUsageOptInLearnMoreArcDisabled');
    }
    if (isChildAccount)
      return this.i18nAdvanced('consolidatedConsentUsageOptInLearnMoreChild');
    return this.i18nAdvanced('consolidatedConsentUsageOptInLearnMore');
  },

  getBackupLearnMoreText_(locale, isChildAccount) {
    if (isChildAccount)
      return this.i18nAdvanced('consolidatedConsentBackupOptInLearnMoreChild');
    return this.i18nAdvanced('consolidatedConsentBackupOptInLearnMore');
  },

  getLocationLearnMoreText_(locale, isChildAccount) {
    if (isChildAccount) {
      return this.i18nAdvanced(
          'consolidatedConsentLocationOptInLearnMoreChild');
    }
    return this.i18nAdvanced('consolidatedConsentLocationOptInLearnMore');
  },

  isArcOptInsHidden_(isArcEnabled, isDemo) {
    return !isArcEnabled || isDemo;
  },

  /**
   * Sets current usage mode.
   * @param {boolean} enabled Defines the state of usage opt in.
   * @param {boolean} managed Defines whether this setting is set by policy.
   */
  SetUsageMode(enabled, managed) {
    this.usageChecked = enabled;
    this.usageManaged_ = managed;
  },

  /**
   * Sets current backup and restore mode.
   * @param {boolean} enabled Defines the state of backup opt in.
   * @param {boolean} managed Defines whether this setting is set by policy.
   */
  setBackupMode(enabled, managed) {
    this.backupChecked = enabled;
    this.backupManaged_ = managed;
  },

  /**
   * Sets current usage of location service opt in mode.
   * @param {boolean} enabled Defines the state of location service opt in.
   * @param {boolean} managed Defines whether this setting is set by policy.
   */
  setLocationMode(enabled, managed) {
    this.locationChecked = enabled;
    this.locationManaged_ = managed;
  },

  /**
   * Opens external URL in popup overlay.
   * @param {string} targetUrl to show in overlay webview.
   */
  showArcTosOverlay(targetUrl) {
    this.$.arcTosOverlayWebview.src = targetUrl;
    this.$.arcTosOverlay.showDialog();
  },

  onGoogleEulaLinkClick_() {
    this.setUIStep(UIState.GOOGLE_EULA);
    this.$.googleEulaOkButton.focus();
  },

  onCrosEulaLinkClick_() {
    this.setUIStep(UIState.CROS_EULA);
    this.$.crosEulaOkButton.focus();
  },

  onArcTosLinkClick_() {
    this.setUIStep(UIState.ARC);
    this.$.ArcTosOkButton.focus();
  },

  onPrivacyPolicyLinkClick_() {
    this.setUIStep(UIState.PRIVACY);
    this.$.privacyOkButton.focus();
  },

  onTermsStepOkClick_() {
    this.setUIStep(UIState.LOADED);
    this.$.acceptButton.focus();
  },

  onUsageLearnMoreClick_() {
    this.$.usageLearnMorePopUp.showDialog();
  },

  onBackupLearnMoreClick_() {
    this.$.backupLearnMorePopUp.showDialog();
  },

  onLocationLearnMoreClick_() {
    this.$.locationLearnMorePopUp.showDialog();
  },

  onFooterLearnMoreClick_() {
    this.$.footerLearnMorePopUp.showDialog();
  },

  onAcceptClick_() {
    chrome.send('ToSAccept', [
      this.usageChecked, this.backupChecked, this.locationChecked,
      this.arcTosContent_
    ]);
  },

  /**
   * On-tap event handler for Retry button.
   *
   * @private
   */
  onRetryClick_() {
    this.setUIStep(UIState.LOADING);
    this.$.retryButton.focus();
    this.loadWebviews_();
  },

  /**
   * On-tap event handler for Back button.
   *
   * @private
   */
  onBack_() {
    this.userActed('back');
  },
});
})();
