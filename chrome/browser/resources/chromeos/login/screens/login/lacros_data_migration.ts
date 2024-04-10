// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for lacros data migration screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_slide.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nMixin, OobeI18nMixinInterface} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './lacros_data_migration.html.js';

enum LacrosDataMigrationStep {
  PROGRESS = 'progress',
  ERROR = 'error',
}

const LacrosDataMigrationScreenElementBase = mixinBehaviors(
    [
      OobeDialogHostBehavior,
      LoginScreenBehavior,
      MultiStepBehavior,
    ],
    OobeI18nMixin(PolymerElement)) as { new (): PolymerElement
      & OobeDialogHostBehaviorInterface
      & OobeI18nMixinInterface
      & LoginScreenBehaviorInterface
      & MultiStepBehaviorInterface,
  };

export class LacrosDataMigrationScreen
    extends LacrosDataMigrationScreenElementBase {
  static get is() {
    return 'lacros-data-migration-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  constructor() {
    super();
  }

  static get properties(): PolymerElementProperties {
    return {
      progressValue: {
        type: Number,
        value: 0,
      },

      canSkip: {
        type: Boolean,
        value: false,
      },

      lowBatteryStatus: {
        type: Boolean,
        value: false,
      },

      requiredSizeStr: {
        type: String,
        value: '',
      },

      showGotoFiles: {
        type: Boolean,
        value: false,
      },
    };
  }

  private progressValue: number;
  private canSkip: boolean;
  private lowBatteryStatus: boolean;
  private requiredSizeStr: string;
  private showGotoFiles: boolean;

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): string {
    return LacrosDataMigrationStep.PROGRESS;
  }

  override get UI_STEPS() {
    return LacrosDataMigrationStep;
  }

  override get EXTERNAL_API(): string[] {
    return [
      'setProgressValue',
      'showSkipButton',
      'setLowBatteryStatus',
      'setFailureStatus',
    ];
  }

  /**
   * Called when the migration failed.
   * @param requiredSizeStr The extra space that users need to free up
   *     to run the migration formatted into a string. Maybe empty, if the
   *     failure is not caused by low disk space.
   * @param showGotoFiles If true, displays the "goto files" button.
   */
  setFailureStatus(requiredSizeStr: string, showGotoFiles: boolean): void {
    this.requiredSizeStr = requiredSizeStr;
    this.showGotoFiles = showGotoFiles;
    this.setUIStep(LacrosDataMigrationStep.ERROR);
  }

  /**
   * Called to update the progress of data migration.
   * @param progress Percentage of data copied so far.
   */
  setProgressValue(progress: number): void {
    this.progressValue = progress;
  }

  /**
   * Called to make the skip button visible.
   */
  showSkipButton(): void {
    this.canSkip = true;
  }

  /**
   * Called on updating low battery status.
   * @param status Whether or not low-battery UI should
   *   show. Specifically, if battery is low and no charger is connected.
   */
  setLowBatteryStatus(status: boolean): void {
    this.lowBatteryStatus = status;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('LacrosDataMigrationScreen');
  }

  private onSkipButtonClicked(): void {
    assert(this.canSkip);
    this.userActed('skip');
  }

  private onCancelButtonClicked(): void {
    this.userActed('cancel');
  }

  private onGotoFilesButtonClicked(): void {
    this.userActed('gotoFiles');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [LacrosDataMigrationScreen.is]: LacrosDataMigrationScreen;
  }
}

customElements.define(LacrosDataMigrationScreen.is, LacrosDataMigrationScreen);
