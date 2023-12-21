// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying encryption migration screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '//resources/polymer/v3_0/paper-styles/color.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';

import {getTemplate} from './encryption_migration.html.js';


/**
 * Enum for the UI states corresponding to sub steps inside migration screen.
 * These values must be kept in sync with
 * EncryptionMigrationScreenView::UIState in C++ code and the order of the
 * enum must be the same.
 */
enum EncryptionMigrationUiState {
  INITIAL = 'initial',
  READY = 'ready',
  MIGRATING = 'migrating',
  MIGRATION_FAILED = 'migration-failed',
  NOT_ENOUGH_SPACE = 'not-enough-space',
}

const EncryptionMigrationBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
    PolymerElement) as { new (): PolymerElement
      & OobeI18nBehaviorInterface
      & LoginScreenBehaviorInterface
      & MultiStepBehaviorInterface,
  };

export class EncryptionMigration extends EncryptionMigrationBase {
  static get is() {
    return 'encryption-migration-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Current migration progress in range [0, 1]. Negative value means that
       * the progress is unknown.
       */
      progress: {
        type: Number,
        value: -1,
      },

      /**
       * Whether the current migration is resuming the previous one.
       */
      isResuming: {
        type: Boolean,
        value: false,
      },

      /**
       * Battery level.
       */
      batteryPercent: {
        type: Number,
        value: 0,
      },

      /**
       * Necessary battery level to start migration in percent.
       */
      necessaryBatteryPercent:{
        type: Number,
        value: 0,
      },

      /**
       * True if the battery level is enough to start migration.
       */
      isEnoughBattery: {
        type: Boolean,
        value: true,
      },

      /**
       * True if the device is charging.
       */
      isCharging: {
        type: Boolean,
        value: false,
      },

      /**
       * True if the migration was skipped.
       */
      isSkipped: {
        type: Boolean,
        value: false,
      },

      /**
       * Formatted string of the current available space size.
       */
      availableSpaceInString: {
        type: String,
        value: '',
      },

      /**
       * Formatted string of the necessary space size for migration.
       */
      necessarySpaceInString: {
        type: String,
        value: '',
      },
    };
  }

  private progress: number;
  private isResuming: boolean;
  private batteryPercent: number;
  private necessaryBatteryPercent: number;
  private isEnoughBattery: boolean;
  private isCharging: boolean;
  private isSkipped: boolean;
  private availableSpaceInString: string;
  private necessarySpaceInString: string;

  constructor() {
    super();
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override get UI_STEPS() {
    return EncryptionMigrationUiState;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return EncryptionMigrationUiState.INITIAL;
  }


  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState() {
    return OOBE_UI_STATE.MIGRATION;
  }

  override get EXTERNAL_API(): string[] {
    return [
      'setUIState',
      'setMigrationProgress',
      'setIsResuming',
      'setBatteryState',
      'setNecessaryBatteryPercent',
      'setSpaceInfoInString',
    ];
  }

  override ready() {
    super.ready();
    this.initializeLoginScreen('EncryptionMigrationScreen');
  }

  /**
   * Updates the migration screen by specifying a state which corresponds
   * to a sub step in the migration process.
   * @param state The UI state to identify a sub step in migration.
   */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  setUIState(state: number): void {
    this.setUIStep(Object.values(EncryptionMigrationUiState)[state]);
  }

  /**
   * Updates the migration progress.
   * @param progress The progress of migration in range [0, 1].
   */
  setMigrationProgress(progress: number): void {
    this.progress = progress;
  }

  /**
   * Updates the migration screen based on whether the migration process
   * is resuming the previous one.
   */
  setIsResuming(isResuming: boolean): void {
    this.isResuming = isResuming;
  }

  /**
   * Updates battery level of the device.
   * @param batteryPercent Battery level in percent.
   * @param isEnoughBattery True if the battery is enough.
   * @param isCharging True if the device is connected to power.
   */
  setBatteryState(batteryPercent: number, isEnoughBattery: boolean,
      isCharging: boolean): void {
    this.batteryPercent = Math.floor(batteryPercent);
    this.isEnoughBattery = isEnoughBattery;
    this.isCharging = isCharging;
  }

  /**
   * Update the necessary battery percent to start migration in the UI.
   * @param necessaryBatteryPercent Necessary battery level.
   */
  setNecessaryBatteryPercent(necessaryBatteryPercent: number): void {
    this.necessaryBatteryPercent = necessaryBatteryPercent;
  }

  /**
   * Updates the string representation of available space size and necessary
   * space size.
   */
  setSpaceInfoInString(availableSpaceSize: string,
      necessarySpaceSize: string): void {
    this.availableSpaceInString = availableSpaceSize;
    this.necessarySpaceInString = necessarySpaceSize;
  }

  /**
   * Returns true if the current migration progress is unknown.
   */
  private isProgressIndeterminate(progress: number): boolean {
    return progress < 0;
  }

  /**
   * Returns true if the 'Update' button should be disabled.
   */
  private isUpdateDisabled(isEnoughBattery: boolean,
      isSkipped: boolean): boolean {
    return !isEnoughBattery || isSkipped;
  }

  /**
   * Returns true if the 'Skip' button on the initial screen should be hidden.
   */
  private isSkipHidden(): boolean {
    // TODO(fukino): Instead of checking the board name here to behave
    // differently, it's recommended to add a command-line flag to Chrome and
    // make session_manager pass it based on a feature-based USE flag which is
    // set in the appropriate board overlays.
    // https://goo.gl/BbBkzg.
    return this.i18n('migrationBoardName').startsWith('kevin');
  }

  /**
   * Computes the label shown under progress bar.
   */
  private computeProgressLabel(locale: string, progress: number): string {
    return this.i18nDynamic(locale,
        'migrationProgressLabel', Math.floor(progress * 100).toString());
  }

  /**
   * Computes the warning label when battery level is not enough.
   */
  private computeBatteryWarningLabel(locale: string,
      batteryPercent: number): string {
    return this.i18nDynamic(locale,
        'migrationBatteryWarningLabel', batteryPercent.toString());
  }

  /**
   * Computes the label to show the necessary battery level for migration.
   */
  private computeNecessaryBatteryLevelLabel(locale: string,
      necessaryBatteryPercent: number): string {
    return this.i18nDynamic(locale,
        'migrationNecessaryBatteryLevelLabel',
        necessaryBatteryPercent.toString());
  }

  /**
   * Computes the label to show the current available space.
   */
  private computeAvailableSpaceLabel(locale: string,
      availableSpaceInString: string): string {
    return this.i18nDynamic(locale,
      'migrationAvailableSpaceLabel', availableSpaceInString);
  }

  /**
   * Computes the label to show the necessary space to start migration.
   */
  private computeNecessarySpaceLabel(locale: string,
      necessarySpaceInString: string): string {
    return this.i18nDynamic(locale,
      'migrationNecessarySpaceLabel', necessarySpaceInString);
  }

  /**
   * Handles click on UPGRADE button.
   */
  private onUpgradeClicked(): void {
    this.userActed('startMigration');
  }

  /**
   * Handles click on SKIP button.
   */
  private onSkipClicked(): void {
    this.isSkipped = true;
    this.userActed('skipMigration');
  }

  /**
   * Handles click on RESTART button.
   */
  private onRestartOnLowStorageClicked(): void {
    this.userActed('requestRestartOnLowStorage');
  }

  /**
   * Handles click on RESTART button on the migration failure screen.
   */
  private onRestartOnFailureClicked(): void {
    this.userActed('requestRestartOnFailure');
  }

  /**
   * Handles click on REPORT AN ISSUE button.
   */
  private onReportAnIssueClicked(): void {
    this.userActed('openFeedbackDialog');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EncryptionMigration.is]: EncryptionMigration;
  }
}

customElements.define(EncryptionMigration.is, EncryptionMigration);
