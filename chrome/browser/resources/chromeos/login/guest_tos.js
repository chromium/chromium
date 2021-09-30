// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview guest tos screen implementation.
 */
(function() {

// Enum that describes the current state of the Consolidated Consent screen
const UIState = {
  LOADING: 'loading',
  LOADED: 'loaded',
  GOOGLE_EULA: 'google-eula',
  CROS_EULA: 'cros-eula',
};

/**
 * URL to use when online page is not available.
 * @type {string}
 */
const EULA_TERMS_URL = 'chrome://terms';

Polymer({
  is: 'guest-tos-element',

  behaviors: [OobeI18nBehavior, MultiStepBehavior, LoginScreenBehavior],

  properties: {},

  ready() {
    this.initializeLoginScreen('GuestTosScreen', {
      resetAllowed: true,
    });
    this.updateLocalizedContent();
  },

  onBeforeShow(data) {
    const googleEulaUrl = data['googleEulaUrl'];
    const crosEulaUrl = data['crosEulaUrl'];

    this.loadEulaWebview_(
        this.$.googleEulaWebview, googleEulaUrl, false /* clear_anchors */);
    this.loadEulaWebview_(
        this.$.crosEulaWebview, crosEulaUrl, true /* clear_anchors */);
  },

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.HIDDEN;
  },

  defaultUIStep() {
    return UIState.LOADING;
  },

  UI_STEPS: UIState,

  updateLocalizedContent() {
    this.$$('#googleEulaLink')
        .addEventListener('click', () => this.onGoogleEulaLinkClick_());
    this.$$('#crosEulaLink')
        .addEventListener('click', () => this.onCrosEulaLinkClick_());
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

  getTerms_(locale) {
    const terms = document.createElement('div');
    terms.innerHTML = this.i18nAdvanced('guestTosTerms', {attrs: ['id']});

    const googleEulaLink = terms.querySelector('#googleEulaLink');
    googleEulaLink.setAttribute('is', 'action-link');
    googleEulaLink.classList.add('oobe-local-link');

    const crosEulaLink = terms.querySelector('#crosEulaLink');
    crosEulaLink.setAttribute('is', 'action-link');
    crosEulaLink.classList.add('oobe-local-link');

    return terms.innerHTML;
  },

  onGoogleEulaLinkClick_() {
    this.setUIStep(UIState.GOOGLE_EULA);
    this.$.googleEulaOkButton.focus();
  },

  onCrosEulaLinkClick_() {
    this.setUIStep(UIState.CROS_EULA);
    this.$.crosEulaOkButton.focus();
  },

  onGoogleEulaContentLoad_() {
    if (this.uiStep == UIState.LOADING) {
      this.setUIStep(UIState.LOADED);
    }
  },

  onTermsStepOkClick_() {
    this.setUIStep(UIState.LOADED);
    this.$.acceptButton.focus();
  },

  onAcceptClick_() {
    this.userActed('accept-button');
  },

  onBackClick_() {
    this.userActed('back-button');
  },

  cancel() {
    this.userActed('cancel');
  },
});
})();
