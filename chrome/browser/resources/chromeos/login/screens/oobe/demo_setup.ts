// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Demo Setup
 * screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/progress_list_item.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './demo_setup.html.js';

/**
 * UI mode for the dialog.
 */
enum DemoSetupUiState {
  PROGRESS = 'progress',
  ERROR = 'error',
}

export const DemoSetupScreenBase = OobeDialogHostMixin(
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement))));

class DemoSetupScreen extends DemoSetupScreenBase {
  static get is() {
    return 'demo-setup-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /** Object mapping step strings to step indices */
      setupSteps_: {
        type: Object,
        value() {
          return loadTimeData.getValue('demoSetupSteps');
        },
      },

      /** Which step index is currently running in Demo Mode setup. */
      currentStepIndex_: {
        type: Number,
        value: -1,
      },

      /** Error message displayed on demoSetupErrorDialog screen. */
      errorMessage_: {
        type: String,
        value: '',
      },

      /** Whether powerwash is required in case of a setup error. */
      isPowerwashRequired_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private setupSteps_: Record<string, number>;
  private currentStepIndex_: number;
  private errorMessage_: string;
  private isPowerwashRequired_: boolean;

  constructor() {
    super();
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('DemoSetupScreen');
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return DemoSetupUiState.PROGRESS;
  }

  override get UI_STEPS() {
    return DemoSetupUiState;
  }

  /** Overridden from LoginScreenBehavior. */
  override get EXTERNAL_API(): string[] {
    return ['setCurrentSetupStep', 'onSetupSucceeded', 'onSetupFailed'];
  }

  override onBeforeShow(): void {
    super.onBeforeShow();
    this.reset();
  }

  /** Resets demo setup flow to the initial screen and starts setup. */
  reset(): void {
    this.setUIStep(DemoSetupUiState.PROGRESS);
    this.userActed('start-setup');
  }

  /** Called after resources are updated. */
  override updateLocalizedContent(): void {
    this.i18nUpdateLocale();
  }

  /**
   * Called at the beginning of a setup step.
   * @param currentStep The new step name.
   */
  setCurrentSetupStep(currentStep: string): void {
    // If new step index not specified, remain unchanged.
    if (this.setupSteps_.hasOwnProperty(currentStep)) {
      this.currentStepIndex_ = this.setupSteps_[currentStep];
    }
  }

  /** Called when demo mode setup succeeded. */
  onSetupSucceeded(): void {
    this.errorMessage_ = '';
  }

  /**
   * Called when demo mode setup failed.
   * @param message Error message to be displayed to the user.
   * @param isPowerwashRequired Whether powerwash is required to
   *     recover from the error.
   */
  onSetupFailed(message: string, isPowerwashRequired: boolean): void {
    this.errorMessage_ = message;
    this.isPowerwashRequired_ = isPowerwashRequired;
    this.setUIStep(DemoSetupUiState.ERROR);
  }

  /**
   * Retry button click handler.
   */
  private onRetryClicked_(): void {
    this.reset();
    chrome.metricsPrivate.recordUserAction('DemoMode.Setup.RetryButtonClicked');
  }

  /**
   * Powerwash button click handler.
   */
  private onPowerwashClicked_(): void {
    this.userActed('powerwash');
  }

  /**
   * Close button click handler.
   */
  private onCloseClicked_(): void {
    // TODO(wzang): Remove this after crbug.com/900640 is fixed.
    if (this.isPowerwashRequired_) {
      return;
    }
    this.userActed('close-setup');
  }

  /**
   * Whether a given step should be rendered on the UI.
   * @param stepName The name of the step (from the enum).
   */
  private shouldShowStep_(stepName: string, setupSteps: Object): boolean {
    return setupSteps.hasOwnProperty(stepName);
  }

  /**
   * Whether a given step is active.
   * @param stepName The name of the step (from the enum).
   */
  private stepIsActive_(
      stepName: string, setupSteps: Record<string, number>,
      currentStepIndex: number): boolean {
    return currentStepIndex === setupSteps[stepName];
  }

  /**
   * Whether a given step is completed.
   * @param stepName The name of the step (from the enum).
   */
  private stepIsCompleted_(
      stepName: string, setupSteps: Record<string, number>,
      currentStepIndex: number): boolean {
    return currentStepIndex > setupSteps[stepName];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [DemoSetupScreen.is]: DemoSetupScreen;
  }
}

customElements.define(DemoSetupScreen.is, DemoSetupScreen);
