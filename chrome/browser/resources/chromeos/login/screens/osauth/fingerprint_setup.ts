// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Fingerprint
 * Enrollment screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {FingerprintProgressElement} from '//resources/ash/common/quick_unlock/fingerprint_progress.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {OobeCrLottie} from '../../components/oobe_cr_lottie.js';

import {getTemplate} from './fingerprint_setup.html.js';


/**
 * These values must be kept in sync with the values in
 * third_party/cros_system_api/dbus/service_constants.h.
 */
enum FingerprintResultType {
  SUCCESS = 0,
  PARTIAL = 1,
  INSUFFICIENT = 2,
  SENSOR_DIRTY = 3,
  TOO_SLOW = 4,
  TOO_FAST = 5,
  IMMOBILE = 6,
}

/**
 * UI mode for the dialog.
 */
enum FingerprintUiState {
  START = 'start',
  PROGRESS = 'progress',
}

const FingerprintSetupBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

interface FingerprintSetupScreenData {
  isChildAccount: boolean;
}

export class FingerprintSetup extends FingerprintSetupBase {
  static get is() {
    return 'fingerprint-setup-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * The percentage of completion that has been received during setup.
       * The value within [0, 100] represents the percent of enrollment
       * completion.
       */
      percentComplete: {
        type: Number,
        value: 0,
        observer: 'onProgressChanged',
      },

      /**
       * Is current finger enrollment complete?
       */
      complete: {
        type: Boolean,
        value: false,
        computed: 'enrollIsComplete(percentComplete)',
      },

      /**
       * Can we add another finger?
       */
      canAddFinger: {
        type: Boolean,
        value: true,
      },

      /**
       * The result of fingerprint enrollment scan.
       */
      scanResult: {
        type: Number,
        value: FingerprintResultType.SUCCESS,
      },

      /**
       * Indicates whether user is a child account.
       */
      isChildAccount: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether Jelly is enabled.
       */
      isDynamicColor: {
        type: Boolean,
        value: loadTimeData.getBoolean('isOobeJellyEnabled'),
      },
    };
  }

  private percentComplete: number;
  private complete: boolean;
  private canAddFinger: boolean;
  private scanResult: FingerprintResultType;
  private isChildAccount: boolean;
  private isDynamicColor: boolean;

  constructor() {
    super();
  }

  override get EXTERNAL_API(): string[] {
    return [
      'onEnrollScanDone',
      'enableAddAnotherFinger',
    ];
  }

  override get UI_STEPS() {
    return FingerprintUiState;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('FingerprintSetupScreen');
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState() {
    return OobeUiState.ONBOARDING;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): string {
    return FingerprintUiState.START;
  }

  override onBeforeShow(data: FingerprintSetupScreenData): void {
    super.onBeforeShow(data);
    this.isChildAccount = data['isChildAccount'];
    this.setAnimationState(true);
  }

  override onBeforeHide(): void {
    super.onBeforeHide();
    this.setAnimationState(false);
  }

  /**
   * Called when a fingerprint enroll scan result is received.
   * @param scanResult Result of the enroll scan.
   * @param isComplete Whether fingerprint enrollment is complete.
   * @param percentComplete Percentage of completion of the enrollment.
   */
  /**
   * TODO(b/321675493) Revamp progress update to validate isComplete
  */
  onEnrollScanDone(
      scanResult: FingerprintResultType, _isComplete: boolean,
      percentComplete: number): void {
    this.setUIStep(FingerprintUiState.PROGRESS);

    const progress = this.shadowRoot?.querySelector('#arc');
    if (progress instanceof FingerprintProgressElement) {
      progress.reset();
    }

    this.percentComplete = percentComplete;
    this.scanResult = scanResult;
  }

  /**
   * Enable/disable add another finger.
   * @param enable True if add another fingerprint is enabled.
   */
  enableAddAnotherFinger(enable: boolean): void {
    this.canAddFinger = enable;
  }

  /**
   * Check whether Add Another button should be shown.
   */
  private isAnotherButtonVisible(
      percentComplete: number, canAddFinger: boolean): boolean {
    return percentComplete >= 100 && canAddFinger;
  }

  /**
   * This is 'on-click' event handler for 'Skip' button for 'START' step.
   */
  private onSkipOnStart(): void {
    this.userActed('setup-skipped-on-start');
  }

  /**
   * This is 'on-click' event handler for 'Skip' button for 'PROGRESS' step.
   */
  private onSkipInProgress(): void {
    this.userActed('setup-skipped-in-flow');
  }

  /**
   * Enable/disable lottie animation.
   * @param playing True if animation should be playing.
   */
  private setAnimationState(playing: boolean): void {
    const animation = this.shadowRoot?.querySelector('#scannerLocationLottie');
    if (animation instanceof OobeCrLottie) {
      animation.playing = playing;
    }

    const progress = this.shadowRoot?.querySelector('#arc');
    if (progress instanceof FingerprintProgressElement) {
      progress.setPlay(playing);
    }
  }

  /**
   * This is 'on-click' event handler for 'Done' button.
   */
  private onDone(): void {
    this.userActed('setup-done');
  }

  /**
   * This is 'on-click' event handler for 'Add another' button.
   */
  private onAddAnother(): void {
    this.percentComplete = 0;
    this.userActed('add-another-finger');
  }

  /**
   * Check whether fingerprint enrollment is in progress.
   */
  private enrollIsComplete(percent: number): boolean {
    return percent >= 100;
  }

  /**
   * Check whether fingerprint scan problem is IMMOBILE.
   */
  private isProblemImmobile(scanResult: number): boolean {
    return scanResult === FingerprintResultType.IMMOBILE;
  }

  /**
   * Check whether fingerprint scan problem is other than IMMOBILE.
   */
  private isProblemOther(scanResult: number): boolean {
    return scanResult !== FingerprintResultType.SUCCESS &&
      scanResult !== FingerprintResultType.IMMOBILE;
  }

  /**
   * Observer for percentComplete.
   */
  private onProgressChanged(newValue: number, oldValue: number): void {
    // Start a new enrollment, so reset all enrollment related states.
    if (newValue === 0) {
      const progress = this.shadowRoot?.querySelector('#arc');
      if (progress instanceof FingerprintProgressElement) {
        progress.reset();
      }
      this.scanResult = FingerprintResultType.SUCCESS;
      return;
    }

    const progress = this.shadowRoot?.querySelector('#arc');
    if (progress instanceof FingerprintProgressElement) {
      progress.setProgress(oldValue, newValue, newValue === 100);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FingerprintSetup.is]: FingerprintSetup;
  }
}

customElements.define(FingerprintSetup.is, FingerprintSetup);
