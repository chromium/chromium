// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for lacros data migration screen.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 */
const LacrosDataMigrationScreenElementBase = Polymer.mixinBehaviors(
    [OobeDialogHostBehavior, LoginScreenBehavior], Polymer.Element);

class LacrosDataMigrationScreen extends LacrosDataMigrationScreenElementBase {
  static get is() {
    return 'lacros-data-migration-element';
  }

  /* #html_template_placeholder */

  constructor() {
    super();
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('LacrosDataMigrationScreen', {
      resetAllowed: false,
    });
  }

  onCancelButtonClicked_() {
    this.userActed('cancel');
  }
}

customElements.define(LacrosDataMigrationScreen.is, LacrosDataMigrationScreen);
