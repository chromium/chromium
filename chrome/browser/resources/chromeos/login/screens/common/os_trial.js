// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for OS trial screen.
 */

/* #js_imports_placeholder */

/**
 * Trial option for setting up the device.
 * @enum {string}
 */
const TrialOption = {
  INSTALL: 'install',
  TRY: 'try',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const OsTrialScreenElementBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @polymer
 */
class OsTrial extends OsTrialScreenElementBase {
  static get is() {
    return 'os-trial-element';
  }

  /* #html_template_placeholder */
  static get properties() {
    return {
      /**
       * The currently selected trial option.
       */
      selectedTrialOption: {
        type: String,
        value: TrialOption.INSTALL,
      },
    };
  }

  constructor() {
    super();
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('OsTrialScreen', {
      resetAllowed: true,
    });
  }

  /**
   * @param {string} locale
   * @private
   */
  getSubtitleHtml_(locale) {
    return this.i18nAdvanced('osTrialSubtitle');
  }

  /**
   * This is the 'on-click' event handler for the 'next' button.
   * @private
   */
  onNextButtonClick_() {
    if (this.selectedTrialOption == TrialOption.TRY)
      this.userActed('os-trial-try');
    else
      this.userActed('os-trial-install');
  }

  /**
   * This is the 'on-click' event handler for the 'back' button.
   * @private
   */
  onBackButtonClick_() {
    this.userActed('os-trial-back');
  }
}
customElements.define(OsTrial.is, OsTrial);
