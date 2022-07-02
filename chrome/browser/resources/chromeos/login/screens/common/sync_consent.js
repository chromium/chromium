// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Sync Consent
 * screen.
 */

/* #js_imports_placeholder */

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const SyncUIState = {
  LOADED: 'loaded',
  LOADING: 'loading',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const SyncConsentScreenElementBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, MultiStepBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @typedef {{
 *   reviewSettingsBox:  CrCheckboxElement,
 * }}
 */
SyncConsentScreenElementBase.$;

class SyncConsentScreen extends SyncConsentScreenElementBase {
  static get is() {
    return 'sync-consent-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /**
       * Flag that determines whether current account type is supervised or not.
       */
      isChildAccount_: Boolean,

      /**
       * Indicates whether user is minor mode user (e.g. under age of 18).
       * @private
       */
      isMinorMode_: Boolean,

      /**
       * Indicates whether ArcAccountRestrictions and LacrosSupport features are
       * enabled.
       * @private
       */
      isArcRestricted_: Boolean,

      /**
       * The text key for the opt-in button (it could vary based on whether
       * the user is in minor mode).
       * @private
       */
      optInButtonTextKey_: {
        type: String,
        computed: 'getOptInButtonTextKey_(isMinorMode_)',
      }
    };
  }

  constructor() {
    super();
    this.UI_STEPS = SyncUIState;

    this.isChildAccount_ = false;
    this.isMinorMode_ = false;
    this.isArcRestricted_ = false;
  }

  get EXTERNAL_API() {
    return ['showLoadedStep', 'setIsMinorMode'];
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  }

  /**
   * Event handler that is invoked just before the screen is shown.
   * @param {Object} data Screen init payload.
   */
  onBeforeShow(data) {
    this.setIsChildAccount(data['isChildAccount']);
    this.isArcRestricted_ = data['isArcRestricted'];
  }

  defaultUIStep() {
    return SyncUIState.LOADING;
  }

  /**
   * Set flag isChildAccount_ value.
   * @param is_child_account Boolean
   */
  setIsChildAccount(is_child_account) {
    this.isChildAccount_ = is_child_account;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('SyncConsentScreen');

    if (this.locale === '') {
      // Update the locale just in case the locale switched between the element
      // loading start and `ready()` event (see https://crbug.com/1289095).
      this.i18nUpdateLocale();
    }
  }

  /**
   * Reacts to changes in loadTimeData.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  }

  /**
   * This is called when SyncScreenBehavior becomes Shown.
   */
  showLoadedStep() {
    this.setUIStep(SyncUIState.LOADED);
  }

  /**
   * Set the minor mode flag, which controls whether we could use nudge
   * techinuque on the UI.
   * @param {boolean} isMinorMode
   */
  setIsMinorMode(isMinorMode) {
    this.isMinorMode_ = isMinorMode;
  }

  /**
   * Continue button is clicked
   * @private
   */
  onSettingsSaveAndContinue_(e, opted_in) {
    assert(e.composedPath());
    chrome.send('login.SyncConsentScreen.continue', [
      opted_in, this.$.reviewSettingsBox.checked, this.getConsentDescription_(),
      this.getConsentConfirmation_(
          /** @type {!Array<!HTMLElement>} */ (e.composedPath()))
    ]);
  }

  onAccepted_(e) {
    this.onSettingsSaveAndContinue_(e, true /* opted_in */);
  }

  onDeclined_(e) {
    this.onSettingsSaveAndContinue_(e, false /* opted_in */);
  }

  /**
   * @param {!Array<!HTMLElement>} path Path of the click event. Must contain
   *     a consent confirmation element.
   * @return {string} The text of the consent confirmation element.
   * @private
   */
  getConsentConfirmation_(path) {
    for (const element of path) {
      if (!element.hasAttribute) {
        continue;
      }

      if (element.hasAttribute('consent-confirmation')) {
        return element.innerHTML.trim();
      }

      // Search down in case of click on a button with description below.
      const labels = element.querySelectorAll('[consent-confirmation]');
      if (labels && labels.length > 0) {
        assert(labels.length == 1);

        let result = '';
        for (const label of labels) {
          result += label.innerHTML.trim();
        }
        return result;
      }
    }
    assertNotReached('No consent confirmation element found.');
    return '';
  }

  /** @return {!Array<string>} Text of the consent description elements. */
  getConsentDescription_() {
    const consentDescription =
        Array.from(this.shadowRoot.querySelectorAll('[consent-description]'))
            .filter(element => element.clientWidth * element.clientHeight > 0)
            .map(element => element.innerHTML.trim());
    assert(consentDescription);
    return consentDescription;
  }

  getReviewSettingText_(locale, isArcRestricted) {
    if (isArcRestricted) {
      return this.i18n('syncConsentReviewSyncOptionsWithArcRestrictedText');
    }
    return this.i18n('syncConsentReviewSyncOptionsText');
  }

  /**
   * @param {boolean} isMinorMode
   * @return {string} The text key of the accept button.
   */
  getOptInButtonTextKey_(isMinorMode) {
    return isMinorMode ? 'syncConsentTurnOnSync' :
                         'syncConsentAcceptAndContinue';
  }
}

customElements.define(SyncConsentScreen.is, SyncConsentScreen);
