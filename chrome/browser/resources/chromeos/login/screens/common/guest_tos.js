// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview guest tos screen implementation.
 */

/* #js_imports_placeholder */

// Enum that describes the current state of the Guest ToS screen
const GuestTosScreenState = {
  LOADING: 'loading',
  LOADED: 'loaded',
  GOOGLE_EULA: 'google-eula',
  CROS_EULA: 'cros-eula',
};

/**
 * URL to use when online page is not available.
 * @type {string}
 */
const GUEST_TOS_EULA_TERMS_URL = 'chrome://terms';

/**
 * Timeout to load online ToS.
 * @type {number}
 */
const GUEST_TOS_ONLINE_LOAD_TIMEOUT_IN_MS = 10000;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const GuestTosScreenElementBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, MultiStepBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @polymer
 */
class GuestTos extends GuestTosScreenElementBase {
  static get is() {
    return 'guest-tos-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      usageChecked: {
        type: Boolean,
        value: true,
      },
    };
  }

  constructor() {
    super();
    this.usageChecked = true;
  }

  /** @override */
  defaultUIStep() {
    return GuestTosScreenState.LOADING;
  }

  get UI_STEPS() {
    return GuestTosScreenState;
  }
  // clang-format on

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('GuestTosScreen');
    this.updateLocalizedContent();
  }

  onBeforeShow(data) {
    const googleEulaUrl = data['googleEulaUrl'];
    const crosEulaUrl = data['crosEulaUrl'];

    this.loadEulaWebview_(
        this.$.googleEulaWebview, googleEulaUrl, false /* clear_anchors */);
    this.loadEulaWebview_(
        this.$.crosEulaWebview, crosEulaUrl, true /* clear_anchors */);
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.HIDDEN;
  }

  updateLocalizedContent() {
    this.shadowRoot.querySelector('#googleEulaLink')
        .addEventListener('click', () => this.onGoogleEulaLinkClick_());
    this.shadowRoot.querySelector('#crosEulaLink')
        .addEventListener('click', () => this.onCrosEulaLinkClick_());
  }

  loadEulaWebview_(webview, online_tos_url, clear_anchors) {
    const loadFailureCallback = () => {
      WebViewHelper.loadUrlContentToWebView(
          webview, GUEST_TOS_EULA_TERMS_URL, WebViewHelper.ContentType.HTML);
    };

    const tosLoader = new WebViewLoader(
        webview, GUEST_TOS_ONLINE_LOAD_TIMEOUT_IN_MS, loadFailureCallback,
        clear_anchors, true /* inject_css */);
    tosLoader.setUrl(online_tos_url);
  }

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
  }

  getUsageLearnMoreText_(locale) {
    return this.i18nAdvanced('guestTosUsageOptInLearnMore');
  }

  onGoogleEulaLinkClick_() {
    this.setUIStep(GuestTosScreenState.GOOGLE_EULA);
    this.$.googleEulaOkButton.focus();
  }

  onCrosEulaLinkClick_() {
    this.setUIStep(GuestTosScreenState.CROS_EULA);
    this.$.crosEulaOkButton.focus();
  }

  onGoogleEulaContentLoad_() {
    if (this.uiStep == GuestTosScreenState.LOADING) {
      this.setUIStep(GuestTosScreenState.LOADED);
    }
  }

  onUsageLearnMoreClick_() {
    this.$.usageLearnMorePopUp.showDialog();
  }

  onTermsStepOkClick_() {
    this.setUIStep(GuestTosScreenState.LOADED);
    this.$.acceptButton.focus();
  }

  onAcceptClick_() {
    chrome.send('GuestToSAccept', [this.usageChecked]);
  }

  onBackClick_() {
    this.userActed('back-button');
  }

  cancel() {
    this.userActed('cancel');
  }
}

customElements.define(GuestTos.is, GuestTos);
