// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe Auto-enrollment check screen implementation.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {LoginScreenBehaviorInterface}
 */
const AutoEnrollmentCheckElementBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    Polymer.Element);

class AutoEnrollmentCheckElement extends AutoEnrollmentCheckElementBase {
  static get is() {
    return 'auto-enrollment-check-element';
  }

  /* #html_template_placeholder */

  ready() {
    super.ready();
    this.initializeLoginScreen('AutoEnrollmentCheckScreen', {
      resetAllowed: true,
    });
  }
}

customElements.define(
    AutoEnrollmentCheckElement.is, AutoEnrollmentCheckElement);
