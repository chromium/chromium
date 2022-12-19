// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Sync Consent
 * screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

// <if expr="_google_chrome">
import '//oobe/sync-consent-icons.m.js';
// </if>

import '../../components/buttons/oobe_text_button.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/hd_iron_icon.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {assert, assertNotReached} from '//resources/ash/common/assert.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';


import {OOBE_UI_STATE, SCREEN_GAIA_SIGNIN} from '../../components/display_manager_types.js';


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
const SyncConsentScreenElementBase = mixinBehaviors(
    [OobeI18nBehavior, MultiStepBehavior, LoginScreenBehavior], PolymerElement);

/**
 * @typedef {{
 *   reviewSettingsBox:  HTMLElement,
 * }}
 */
SyncConsentScreenElementBase.$;

class SyncConsentScreen extends SyncConsentScreenElementBase {
  static get is() {
    return 'sync-consent-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
      },
    };
  }

  constructor() {
    super();
    this.UI_STEPS = SyncUIState;

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
    this.isArcRestricted_ = data['isArcRestricted'];
  }

  defaultUIStep() {
    return SyncUIState.LOADING;
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
    this.userActed([
      'continue',
      opted_in,
      this.$.reviewSettingsBox.checked,
      this.getConsentDescription_(),
      this.getConsentConfirmation_(
          /** @type {!Array<!HTMLElement>} */ (e.composedPath())),
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
