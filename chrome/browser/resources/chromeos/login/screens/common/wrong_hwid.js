// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview wrong HWID screen implementation.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const WrongHWIDBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @polymer
 */
class WrongHWID extends WrongHWIDBase {
  static get is() {
    return 'wrong-hwid-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {};
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('WrongHWIDMessageScreen', {
      resetAllowed: true,
    });
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.WRONG_HWID_WARNING;
  }

  onSkip_() {
    this.userActed('skip-screen');
  }

  formattedFirstPart_(locale) {
    return this.i18nAdvanced('wrongHWIDMessageFirstPart');
  }
}

customElements.define(WrongHWID.is, WrongHWID);
