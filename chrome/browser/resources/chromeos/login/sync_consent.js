// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Sync Consent
 * screen.
 */

'use strict';

(function() {

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const UIState = {
  NO_SPLIT: 'no-split',
  SPLIT: 'split',
  LOADING: 'loading',
};

Polymer({
  is: 'sync-consent-element',

  behaviors: [
    OobeI18nBehavior,
    OobeDialogHostBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  properties: {
    /**
     * Flag that determines whether current account type is supervised or not.
     */
    isChildAccount_: Boolean,

    /** @private */
    splitSettingsSyncEnabled_: {
      type: Boolean,
      value: false,
    },

    /**
     * Indicates whether user is minor mode user (e.g. under age of 18).
     */
    isMinorMode_: {
      type: Boolean,
      value: false,
    },

    /**
     * The text key for the opt-in button (it could vary based on whether
     * the user is in minor mode).
     */
    optInButtonTextKey_: {
      type: String,
      computed: 'getOptInButtonTextKey_(isMinorMode_)',
    }
  },

  EXTERNAL_API: [
    'setThrobberVisible',
    'setIsMinorMode',
  ],

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  },

  /**
   * Event handler that is invoked just before the screen is shown.
   * @param {Object} data Screen init payload.
   */
  onBeforeShow(data) {
    this.setIsChildAccount(data['isChildAccount']);
    this.splitSettingsSyncEnabled_ = data['splitSettingsSyncEnabled'];
  },

  /**
   * Event handler that is invoked just before the screen is hidden.
   */
  onBeforeHide() {
    this.setThrobberVisible(false /*visible*/);
  },

  defaultUIStep() {
    return this.getDefaultUIStep_();
  },

  UI_STEPS: UIState,

  /**
   * Set flag isChildAccount_ value.
   * @param is_child_account Boolean
   */
  setIsChildAccount(is_child_account) {
    this.isChildAccount_ = is_child_account;
  },

  /** @override */
  ready() {
    this.initializeLoginScreen('SyncConsentScreen', {
      resetAllowed: true,
    });
    this.updateLocalizedContent();
  },

  /**
   * Reacts to changes in loadTimeData.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  /**
   * This is called to show/hide the loading UI.
   * @param {boolean} visible whether to show loading UI.
   */
  setThrobberVisible(visible) {
    if (visible) {
      this.setUIStep(UIState.LOADING);
    } else {
      this.setUIStep(this.getDefaultUIStep_());
    }
  },

  /**
   * Set the minor mode flag, which controls whether we could use nudge
   * techinuque on the UI.
   * @param {boolean} isMinorMode
   */
  setIsMinorMode(isMinorMode) {
    this.isMinorMode_ = isMinorMode;
  },

  /**
   * Returns split settings sync version or regular version depending on if
   * split settings sync is enabled.
   * @private
   */
  getDefaultUIStep_() {
    return this.splitSettingsSyncEnabled_ ? UIState.SPLIT : UIState.NO_SPLIT;
  },

  /**
   * Continue button click handler for pre-SplitSettingsSync.
   * @private
   */
  onSettingsSaveAndContinue_(e, opted_in) {
    assert(e.path);
    assert(!this.splitSettingsSyncEnabled_);
    chrome.send('login.SyncConsentScreen.nonSplitSettingsContinue', [
      opted_in, this.$.reviewSettingsBox.checked, this.getConsentDescription_(),
      this.getConsentConfirmation_(e.path)
    ]);
  },

  onNonSplitSettingsAccepted_(e) {
    this.onSettingsSaveAndContinue_(e, true /* opted_in */);
  },

  onNonSplitSettingsDeclined_(e) {
    this.onSettingsSaveAndContinue_(e, false /* opted_in */);
  },

  /**
   * Accept button handler for SplitSettingsSync.
   * @param {!Event} event
   * @private
   */
  onAcceptTap_(event) {
    assert(this.splitSettingsSyncEnabled_);
    assert(event.path);
    chrome.send('login.SyncConsentScreen.acceptAndContinue', [
      this.getConsentDescription_(), this.getConsentConfirmation_(event.path)
    ]);
  },

  /**
   * Decline button handler for SplitSettingsSync.
   * @param {!Event} event
   * @private
   */
  onDeclineTap_(event) {
    assert(this.splitSettingsSyncEnabled_);
    assert(event.path);
    chrome.send('login.SyncConsentScreen.declineAndContinue', [
      this.getConsentDescription_(), this.getConsentConfirmation_(event.path)
    ]);
  },

  /**
   * @param {!Array<!HTMLElement>} path Path of the click event. Must contain
   *     a consent confirmation element.
   * @return {string} The text of the consent confirmation element.
   * @private
   */
  getConsentConfirmation_(path) {
    for (let element of path) {
      if (!element.hasAttribute)
        continue;

      if (element.hasAttribute('consent-confirmation'))
        return element.innerHTML.trim();

      // Search down in case of click on a button with description below.
      let labels = element.querySelectorAll('[consent-confirmation]');
      if (labels && labels.length > 0) {
        assert(labels.length == 1);

        let result = '';
        for (let label of labels) {
          result += label.innerHTML.trim();
        }
        return result;
      }
    }
    assertNotReached('No consent confirmation element found.');
    return '';
  },

  /** @return {!Array<string>} Text of the consent description elements. */
  getConsentDescription_() {
    let consentDescription =
      Array.from(this.shadowRoot.querySelectorAll('[consent-description]'))
        .filter(element => element.clientWidth * element.clientHeight > 0)
        .map(element => element.innerHTML.trim());
    assert(consentDescription);
    return consentDescription;
  },

  /**
   * @param {boolean} isMinorMode
   * @return {string} The text key of the accept button.
   */
  getOptInButtonTextKey_(isMinorMode) {
    return isMinorMode ? 'syncConsentTurnOnSync' :
                         'syncConsentAcceptAndContinue';
  },
});
})();
