// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Packaged License screen.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const PackagedLicenseScreenBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @typedef {{
 *   packagedLicenseDialog:  OobeAdaptiveDialogElement,
 * }}
 */
PackagedLicenseScreenBase.$;

class PackagedLicenseScreen extends PackagedLicenseScreenBase {
  static get is() {
    return 'packaged-license-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {};
  }

  constructor() {
    super();
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('PackagedLicenseScreen', {resetAllowed: true});
  }

  /**
   * Returns the control which should receive initial focus.
   */
  get defaultControl() {
    return this.$.packagedLicenseDialog;
  }

  /**
   * On-tap event handler for Don't Enroll button.
   * @private
   */
  onDontEnrollButtonPressed_() {
    this.userActed('dont-enroll');
  }

  /**
   * On-tap event handler for Enroll button.
   * @private
   */
  onEnrollButtonPressed_() {
    this.userActed('enroll');
  }
}

customElements.define(PackagedLicenseScreen.is, PackagedLicenseScreen);
