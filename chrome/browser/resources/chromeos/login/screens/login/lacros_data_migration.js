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
    [OobeDialogHostBehavior, OobeI18nBehavior, LoginScreenBehavior],
    Polymer.Element);

class LacrosDataMigrationScreen extends LacrosDataMigrationScreenElementBase {
  static get is() {
    return 'lacros-data-migration-element';
  }

  /* #html_template_placeholder */

  constructor() {
    super();
    this.progressValue_ = 0;
  }

  static get properties() {
    return {
      progressValue_: {type: Number},
    };
  }

  get EXTERNAL_API() {
    return ['setProgressValue'];
  }

  /**
   * Called to update the progress of data migration.
   * @param {number} progress Percentage of data copied so far.
   */
  setProgressValue(progress) {
    this.progressValue_ = progress;
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
