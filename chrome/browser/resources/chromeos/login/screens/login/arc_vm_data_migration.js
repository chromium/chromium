// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for ARCVM /data migration screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '//resources/polymer/v3_0/paper-styles/color.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/oobe_icons.html.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';

// Keep in sync with ArcVmDataMigrationScreenView::UIState.
var ArcVmDataMigrationUIState = {
  LOADING: 'loading',
  WELCOME: 'welcome',
  RESUM: 'resume',
  PROGRESS: 'progress',
  SUCCESS: 'success',
  FAILURE: 'failure',
};

// Keep in sync with kUserAction* in arc_vm_data_migration_screen.cc.
var ArcVmDataMigrationUserAction = {
  SKIP: 'skip',
  UPDATE: 'update',
  RESUME: 'resume',
  FINISH: 'finish',
  REPORT: 'report',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const ArcVmDataMigrationScreenElementBase = mixinBehaviors(
    [
      OobeDialogHostBehavior,
      OobeI18nBehavior,
      LoginScreenBehavior,
      MultiStepBehavior,
    ],
    PolymerElement);

class ArcVmDataMigrationScreen extends ArcVmDataMigrationScreenElementBase {
  static get is() {
    return 'arc-vm-data-migration-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      hasEnoughFreeDiskSpace: Boolean,
      requiredFreeDiskSpaceInString: String,
      minimumBatteryPercent: Number,
      hasEnoughBattery: Boolean,
      isConnectedToCharger: Boolean,
      migrationProgress: Number,
      estimatedRemainingTimeInString: String,
    };
  }

  constructor() {
    super();
    this.hasEnoughFreeDiskSpace = true;
    this.requiredFreeDiskSpaceInString = '';
    this.minimumBatteryPercent = 0;
    this.hasEnoughBattery = true;
    this.isConnectedToCharger = true;
    this.migrationProgress = -1;
    this.estimatedRemainingTimeInString = '';
  }

  defaultUIStep() {
    return ArcVmDataMigrationUIState.LOADING;
  }

  get UI_STEPS() {
    return ArcVmDataMigrationUIState;
  }

  get EXTERNAL_API() {
    return [
      'setUIState',
      'setRequiredFreeDiskSpace',
      'setMinimumBatteryPercent',
      'setBatteryState',
      'setMigrationProgress',
      'setEstimatedRemainingTime',
    ];
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('ArcVmDataMigrationScreen');
  }

  getOobeUIInitialState() {
    return OOBE_UI_STATE.MIGRATION;
  }

  setUIState(state) {
    this.setUIStep(Object.values(ArcVmDataMigrationUIState)[state]);
  }

  setRequiredFreeDiskSpace(requiredFreeDiskSpaceInString) {
    this.hasEnoughFreeDiskSpace = false;
    this.requiredFreeDiskSpaceInString = requiredFreeDiskSpaceInString;
  }

  setMinimumBatteryPercent(minimumBatteryPercent) {
    this.minimumBatteryPercent = Math.floor(minimumBatteryPercent);
  }

  setBatteryState(hasEnoughBattery, isConnectedToCharger) {
    this.hasEnoughBattery = hasEnoughBattery;
    this.isConnectedToCharger = isConnectedToCharger;
  }

  setMigrationProgress(migrationProgress) {
    this.migrationProgress = Math.floor(migrationProgress);
  }

  setEstimatedRemainingTime(estimatedRemainingTimeInString) {
    this.estimatedRemainingTimeInString = estimatedRemainingTimeInString;
  }

  shouldDisableUpdateButton_(hasEnoughFreeDiskSpace, hasEnoughBattery) {
    return !hasEnoughFreeDiskSpace || !hasEnoughBattery;
  }

  isProgressIndeterminate_(migrationProgress) {
    return migrationProgress < 0;
  }

  onSkipButtonClicked_() {
    this.userActed(ArcVmDataMigrationUserAction.SKIP);
  }

  onUpdateButtonClicked_() {
    this.userActed(ArcVmDataMigrationUserAction.UPDATE);
  }

  onResumeButtonClicked_() {
    this.userActed(ArcVmDataMigrationUserAction.RESUME);
  }

  onFinishButtonClicked_() {
    this.userActed(ArcVmDataMigrationUserAction.FINISH);
  }

  onReportButtonClicked_() {
    this.userActed(ArcVmDataMigrationUserAction.REPORT);
  }
}

customElements.define(ArcVmDataMigrationScreen.is, ArcVmDataMigrationScreen);
