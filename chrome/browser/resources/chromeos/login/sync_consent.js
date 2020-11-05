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
     * The device type (e.g. "Chromebook" or "Chromebox").
     * TODO(jamescook): Delete this after M85 once we're sure UX doesn't want
     * the device type in the dialog.
     * @private
     */
    deviceType_: String,
  },

  EXTERNAL_API: ['setThrobberVisible'],

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
    this.setDeviceType(data['deviceType']);
    this.splitSettingsSyncEnabled_ = data['splitSettingsSyncEnabled'];
    this.setUIStep(this.defaultUIStep());
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

  /**
   * @param deviceType {string} The device type (e.g. "Chromebook").
   */
  setDeviceType(deviceType) {
    this.deviceType_ = deviceType;
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
  onSettingsSaveAndContinue_(e) {
    assert(e.path);
    assert(!this.splitSettingsSyncEnabled_);
    if (this.$.reviewSettingsBox.checked) {
      chrome.send('login.SyncConsentScreen.continueAndReview', [
        this.getConsentDescription_(), this.getConsentConfirmation_(e.path)
      ]);
    } else {
      chrome.send('login.SyncConsentScreen.continueWithDefaults', [
        this.getConsentDescription_(), this.getConsentConfirmation_(e.path)
      ]);
    }
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
});
})();
