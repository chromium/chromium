// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe Assistant OptIn Flow screen implementation.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 */
const AssistantOptinBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @polymer
 */
class AssistantOptin extends AssistantOptinBase {
  static get is() {
    return 'assistant-optin-element';
  }

  /* #html_template_placeholder */

  get EXTERNAL_API() {
    return [
      'reloadContent', 'addSettingZippy', 'showNextScreen',
      'onVoiceMatchUpdate', 'onValuePropUpdate'
    ];
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('AssistantOptInFlowScreen');
  }

  /**
   * Returns default event target element.
   * @type {Object}
   */
  get defaultControl() {
    return this.$.card;
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {Object} data Screen init payload
   * @suppress {missingProperties}
   */
  onBeforeShow(data) {
    this.$.card.onShow();
  }

  /**
   * Reloads localized strings.
   * @param {!Object} data New dictionary with i18n values.
   * @suppress {missingProperties}
   */
  reloadContent(data) {
    this.$.card.reloadContent(data);
  }

  /**
   * Add a setting zippy object in the corresponding screen.
   * @param {string} type type of the setting zippy.
   * @param {!Object} data String and url for the setting zippy.
   * @suppress {missingProperties}
   */
  addSettingZippy(type, data) {
    this.$.card.addSettingZippy(type, data);
  }

  /**
   * Show the next screen in the flow.
   * @suppress {missingProperties}
   */
  showNextScreen() {
    this.$.card.showNextScreen();
  }

  /**
   * Called when the Voice match state is updated.
   * @param {string} state the voice match state.
   * @suppress {missingProperties}
   */
  onVoiceMatchUpdate(state) {
    this.$.card.onVoiceMatchUpdate(state);
  }

  /**
   * Called to show the next settings when there are multiple unbundled
   * activity control settings in the Value prop screen.
   */
  onValuePropUpdate() {
    this.$.card.onValuePropUpdate();
  }
}

customElements.define(AssistantOptin.is, AssistantOptin);
