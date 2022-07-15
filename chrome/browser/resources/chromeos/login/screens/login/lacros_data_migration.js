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
 * @implements {MultiStepBehaviorInterface}
 */
const LacrosDataMigrationScreenElementBase = Polymer.mixinBehaviors(
    [
      OobeDialogHostBehavior,
      OobeI18nBehavior,
      LoginScreenBehavior,
      MultiStepBehavior,
    ],
    Polymer.Element);

class LacrosDataMigrationScreen extends LacrosDataMigrationScreenElementBase {
  static get is() {
    return 'lacros-data-migration-element';
  }

  /* #html_template_placeholder */

  constructor() {
    super();
    this.progressValue_ = 0;
    this.canSkip_ = false;
    this.lowBatteryStatus_ = false;
    this.requiredSizeStr_ = '';
    this.showGotoFiles_ = false;
  }

  static get properties() {
    return {
      progressValue_: {type: Number},

      canSkip_: {type: Boolean},
      lowBatteryStatus_: {type: Boolean},
      requiredSizeStr_: {type: String},
      showGotoFiles_: {type: Boolean},
    };
  }

  defaultUIStep() {
    return 'progress';
  }

  get UI_STEPS() {
    return {
      PROGRESS: 'progress',
      ERROR: 'error',
    };
  }

  get EXTERNAL_API() {
    return [
      'setProgressValue',
      'showSkipButton',
      'setLowBatteryStatus',
      'setFailureStatus',
    ];
  }

  /**
   * Called when the migration failed.
   * @param {string} requiredSizeStr The extra space that users need to free up
   *     to run the migration formatted into a string. Maybe empty, if the
   *     failure is not caused by low disk space.
   * @param {boolean} showGotoFiles If true, displays the "goto files" button.
   */
  setFailureStatus(requiredSizeStr, showGotoFiles) {
    this.requiredSizeStr_ = requiredSizeStr;
    this.showGotoFiles_ = showGotoFiles;
    this.setUIStep('error');
  }

  /**
   * Called to update the progress of data migration.
   * @param {number} progress Percentage of data copied so far.
   */
  setProgressValue(progress) {
    this.progressValue_ = progress;
  }

  /**
   * Called to make the skip button visible.
   */
  showSkipButton() {
    this.canSkip_ = true;
  }

  /**
   * Called on updating low battery status.
   * @param {boolean} status Whether or not low-battery UI should
   *   show. Specifically, if battery is low and no charger is connected.
   */
  setLowBatteryStatus(status) {
    this.lowBatteryStatus_ = status;
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('LacrosDataMigrationScreen');
  }

  onSkipButtonClicked_() {
    assert(this.canSkip_);
    this.userActed('skip');
  }

  onCancelButtonClicked_() {
    this.userActed('cancel');
  }

  onGotoFilesButtonClicked_() {
    this.userActed('gotoFiles');
  }
}

customElements.define(LacrosDataMigrationScreen.is, LacrosDataMigrationScreen);
