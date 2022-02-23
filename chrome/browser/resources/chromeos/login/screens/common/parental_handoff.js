// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Parental Handoff screen.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {LoginScreenBehaviorInterface}
 */
const ParentalHandoffElementBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @typedef {{
 *   parentalHandoffDialog:  OobeAdaptiveDialogElement,
 * }}
 */
ParentalHandoffElementBase.$;

class ParentalHandoff extends ParentalHandoffElementBase {
  static get is() {
    return 'parental-handoff-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /**
       * The username to be displayed
       */
      username_: {
        type: String,
      },
    };
  }

  constructor() {
    super();
    this.username_ = '';
  }

  /** @override */
  get EXTERNAL_API() {
    return [];
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {Object} data Screen init payload
   */
  onBeforeShow(data) {
    if ('username' in data) {
      this.username_ = data.username;
    }
    this.$.parentalHandoffDialog.focus();
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('ParentalHandoffScreen', {
      resetAllowed: true,
    });
  }

  /*
   * Executed on language change.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  }

  /**
   * On-tap event handler for Next button.
   *
   * @private
   */
  onNextButtonPressed_() {
    this.userActed('next');
  }
}

customElements.define(ParentalHandoff.is, ParentalHandoff);
