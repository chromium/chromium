// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying encryption migration screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import type {String16} from '//resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {EncryptionMigrationPage_UIState, EncryptionMigrationPageCallbackRouter, EncryptionMigrationPageHandlerRemote} from '../../mojom-webui/screens_login.mojom-webui.js';
import {OobeScreensFactoryBrowserProxy} from '../../oobe_screens_factory_proxy.js';

import {getTemplate} from './encryption_migration.html.js';


/**
 * Enum for the UI states corresponding to sub steps inside migration screen.
 * These values must be kept in sync with
 * EncryptionMigrationPage::UIState in mojo
 */
enum EncryptionMigrationUiState {
  INITIAL = 'initial',
  READY = 'ready',
  MIGRATING = 'migrating',
  MIGRATION_FAILED = 'migration-failed',
  NOT_ENOUGH_SPACE = 'not-enough-space',
}

const EncryptionMigrationBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

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
  private callbackRouter: EncryptionMigrationPageCallbackRouter;
  private handler: EncryptionMigrationPageHandlerRemote;

  constructor() {
    super();
    this.callbackRouter = new EncryptionMigrationPageCallbackRouter();
    this.handler = new EncryptionMigrationPageHandlerRemote();
    OobeScreensFactoryBrowserProxy.getInstance()
        .screenFactory
        .establishEncryptionMigrationScreenPipe(
            this.handler.$.bindNewPipeAndPassReceiver())
        .then((response: any) => {
          this.callbackRouter.$.bindHandle(response.pending.handle);
        });

    this.callbackRouter.setUIState.addListener(this.setUIState.bind(this));
    this.callbackRouter.setMigrationProgress.addListener(
        this.setMigrationProgress.bind(this));
    this.callbackRouter.setIsResuming.addListener(
        this.setIsResuming.bind(this));
    this.callbackRouter.setBatteryState.addListener(
        this.setBatteryState.bind(this));
    this.callbackRouter.setNecessaryBatteryPercent.addListener(
        this.setNecessaryBatteryPercent.bind(this));
    this.callbackRouter.setSpaceInfoInString.addListener(
        this.setSpaceInfoInString.bind(this));
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
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.MIGRATION;
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
  setUIState(value: EncryptionMigrationPage_UIState): void {
    switch (value) {
      case EncryptionMigrationPage_UIState.kInitial:
        this.setUIStep(EncryptionMigrationUiState.INITIAL);
        break;
      case EncryptionMigrationPage_UIState.kReady:
        this.setUIStep(EncryptionMigrationUiState.READY);
        break;
      case EncryptionMigrationPage_UIState.kMigrating:
        this.setUIStep(EncryptionMigrationUiState.MIGRATING);
        break;
      case EncryptionMigrationPage_UIState.kMigratingFailed:
        this.setUIStep(EncryptionMigrationUiState.MIGRATION_FAILED);
        break;
      case EncryptionMigrationPage_UIState.kNotEnoughStorage:
        this.setUIStep(EncryptionMigrationUiState.NOT_ENOUGH_SPACE);
        break;
    }
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
  setSpaceInfoInString(
      availableSpaceSize: String16, necessarySpaceSize: String16): void {
    this.availableSpaceInString =
        String.fromCharCode(...availableSpaceSize.data);
    this.necessarySpaceInString =
        String.fromCharCode(...necessarySpaceSize.data);
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
    this.handler.onStartMigration();
  }

  /**
   * Handles click on SKIP button.
   */
  private onSkipClicked(): void {
    this.isSkipped = true;
    this.handler.onSkipMigration();
  }

  /**
   * Handles click on RESTART button.
   */
  private onRestartOnLowStorageClicked(): void {
    this.handler.onRequestRestartOnLowStorage();
  }

  /**
   * Handles click on RESTART button on the migration failure screen.
   */
  private onRestartOnFailureClicked(): void {
    this.handler.onRequestRestartOnFailure();
  }

  /**
   * Handles click on REPORT AN ISSUE button.
   */
  private onReportAnIssueClicked(): void {
    this.handler.onOpenFeedbackDialog();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EncryptionMigration.is]: EncryptionMigration;
  }
}

customElements.define(EncryptionMigration.is, EncryptionMigration);
