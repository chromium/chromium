// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="../assistant_optin/assistant_optin_flow.js">

/**
 * @fileoverview Oobe Assistant OptIn Flow screen implementation.
 */

Polymer({
  is: 'assistant-optin-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  EXTERNAL_API: [
    'reloadContent',
    'addSettingZippy',
    'showNextScreen',
    'onVoiceMatchUpdate',
  ],

  ready() {
    this.initializeLoginScreen('AssistantOptInFlowScreen', {
      resetAllowed: false,
    });
  },

  /**
   * Returns default event target element.
   * @type {Object}
   */
  get defaultControl() {
    return this.$.card;
  },

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  },

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {Object} data Screen init payload
   * @suppress {missingProperties}
   */
  onBeforeShow(data) {
    this.$.card.onShow();
  },

  /**
   * Reloads localized strings.
   * @param {!Object} data New dictionary with i18n values.
   * @suppress {missingProperties}
   */
  reloadContent(data) {
    this.$.card.reloadContent(data);
  },

  /**
   * Add a setting zippy object in the corresponding screen.
   * @param {string} type type of the setting zippy.
   * @param {!Object} data String and url for the setting zippy.
   * @suppress {missingProperties}
   */
  addSettingZippy(type, data) {
    this.$.card.addSettingZippy(type, data);
  },

  /**
   * Show the next screen in the flow.
   * @suppress {missingProperties}
   */
  showNextScreen() {
    this.$.card.showNextScreen();
  },

  /**
   * Called when the Voice match state is updated.
   * @param {string} state the voice match state.
   * @suppress {missingProperties}
   */
  onVoiceMatchUpdate(state) {
    this.$.card.onVoiceMatchUpdate(state);
  },
});
