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

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nMixin, OobeI18nMixinInterface} from '../../components/mixins/oobe_i18n_mixin.js';
import {OobeUiState} from '../../components/display_manager_types.js';

import {getTemplate} from './arc_vm_data_migration.html.js';

// Keep in sync with ArcVmDataMigrationScreenView::UIState.
enum ArcVmDataMigrationUiState {
  LOADING = 'loading',
  WELCOME = 'welcome',
  RESUM = 'resume',
  PROGRESS = 'progress',
  SUCCESS = 'success',
  FAILURE = 'failure',
}

// Keep in sync with kUserAction* in arc_vm_data_migration_screen.cc.
enum ArcVmDataMigrationUserAction {
  SKIP = 'skip',
  UPDATE = 'update',
  RESUME = 'resume',
  FINISH = 'finish',
  REPORT = 'report',
}

const ArcVmDataMigrationScreenElementBase = mixinBehaviors(
  [
    OobeDialogHostBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],
  OobeI18nMixin(PolymerElement)) as {
  new (): PolymerElement & OobeDialogHostBehaviorInterface &
      OobeI18nMixinInterface & LoginScreenBehaviorInterface &
      MultiStepBehaviorInterface,
};

export class ArcVmDataMigrationScreen extends
    ArcVmDataMigrationScreenElementBase {
  static get is() {
    return 'arc-vm-data-migration-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      hasEnoughFreeDiskSpace: {
        type: Boolean,
        value: true,
      },

      requiredFreeDiskSpaceInString: {
        type: String,
        value: '',
      },

      minimumBatteryPercent: {
        type: Number,
        value: 0,
      },

      hasEnoughBattery: {
        type: Boolean,
        value: true,
      },

      isConnectedToCharger: {
        type: Boolean,
        value: true,
      },

      migrationProgress: {
        type: Number,
        value: -1,
      },

      estimatedRemainingTimeInString: {
        type: String,
        value: '',
      },
    };
  }

  private hasEnoughFreeDiskSpace: boolean;
  private requiredFreeDiskSpaceInString: string;
  private minimumBatteryPercent: number;
  private hasEnoughBattery: boolean;
  private isConnectedToCharger: boolean;
  private migrationProgress: number;
  private estimatedRemainingTimeInString: string;

  constructor() {
    super();
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): string {
    return ArcVmDataMigrationUiState.LOADING;
  }

  override get UI_STEPS() {
    return ArcVmDataMigrationUiState;
  }

  override get EXTERNAL_API(): string[] {
    return [
      'setUIState',
      'setRequiredFreeDiskSpace',
      'setMinimumBatteryPercent',
      'setBatteryState',
      'setMigrationProgress',
      'setEstimatedRemainingTime',
    ];
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('ArcVmDataMigrationScreen');
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.MIGRATION;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  setUIState(state: number): void {
    this.setUIStep(Object.values(ArcVmDataMigrationUiState)[state]);
  }

  setRequiredFreeDiskSpace(requiredFreeDiskSpaceInString: string): void {
    this.hasEnoughFreeDiskSpace = false;
    this.requiredFreeDiskSpaceInString = requiredFreeDiskSpaceInString;
  }

  setMinimumBatteryPercent(minimumBatteryPercent: number): void {
    this.minimumBatteryPercent = Math.floor(minimumBatteryPercent);
  }

  setBatteryState(hasEnoughBattery: boolean, isConnectedToCharger: boolean):
      void {
    this.hasEnoughBattery = hasEnoughBattery;
    this.isConnectedToCharger = isConnectedToCharger;
  }

  setMigrationProgress(migrationProgress: number): void {
    this.migrationProgress = Math.floor(migrationProgress);
  }

  setEstimatedRemainingTime(estimatedRemainingTimeInString: string): void {
    this.estimatedRemainingTimeInString = estimatedRemainingTimeInString;
  }

  private shouldDisableUpdateButton(
      hasEnoughFreeDiskSpace: boolean, hasEnoughBattery: boolean): boolean {
    return !hasEnoughFreeDiskSpace || !hasEnoughBattery;
  }

  private isProgressIndeterminate(migrationProgress: number): boolean {
    return migrationProgress < 0;
  }

  private onSkipButtonClicked(): void {
    this.userActed(ArcVmDataMigrationUserAction.SKIP);
  }

  private onUpdateButtonClicked(): void {
    this.userActed(ArcVmDataMigrationUserAction.UPDATE);
  }

  private onResumeButtonClicked(): void {
    this.userActed(ArcVmDataMigrationUserAction.RESUME);
  }

  private onFinishButtonClicked(): void {
    this.userActed(ArcVmDataMigrationUserAction.FINISH);
  }

  private onReportButtonClicked(): void {
    this.userActed(ArcVmDataMigrationUserAction.REPORT);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ArcVmDataMigrationScreen.is]: ArcVmDataMigrationScreen;
  }
}

customElements.define(ArcVmDataMigrationScreen.is, ArcVmDataMigrationScreen);
