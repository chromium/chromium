// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/lottie_renderer/lottie-renderer.js';
import 'chrome://resources/ash/common/quick_unlock/fingerprint_progress.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {FingerprintProgressElement} from 'chrome://resources/ash/common/quick_unlock/fingerprint_progress.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';

import {FingerprintBrowserProxy, FingerprintBrowserProxyImpl, FingerprintResultType, FingerprintScan} from './fingerprint_browser_proxy.js';
import {getTemplate} from './setup_fingerprint_dialog.html.js';


/**
 * The steps in the fingerprint setup flow.
 */
export enum FingerprintSetupStep {
  LOCATE_SCANNER = 1,  // The user needs to locate the scanner.
  MOVE_FINGER = 2,     // The user needs to move finger around the scanner.
  READY = 3,           // The scanner has read the fingerprint successfully.
}

/**
 * The amount of milliseconds after a successful but not completed scan before
 * a message shows up telling the user to scan their finger again.
 */
const SHOW_TAP_SENSOR_MESSAGE_DELAY_MS = 2000;

const SettingsSetupFingerprintDialogElementBase =
    I18nMixin(WebUiListenerMixin(PolymerElement));

export interface SettingsSetupFingerprintDialogElement {
  $: {
    dialog: CrDialogElement,
    arc: FingerprintProgressElement,
  };
}

export class SettingsSetupFingerprintDialogElement extends
    SettingsSetupFingerprintDialogElementBase {
  static get is() {
    return 'settings-setup-fingerprint-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether add another finger is allowed.
       */
      allowAddAnotherFinger: {
        type: Boolean,
        value: true,
      },

      /**
       * Authentication token provided by settings-fingerprint-list-subpage
       */
      authToken: {
        type: String,
        value: '',
      },

      /**
       * The problem message to display.
       */
      problemMessage_: {
        type: String,
        value: '',
      },

      /**
       * The setup phase we are on.
       */
      step_: {type: Number, value: FingerprintSetupStep.LOCATE_SCANNER},

      /**
       * The percentage of completion that has been received during setup.
       * This is used to approximate the progress of the setup.
       * The value within [0, 100] represents the percent of enrollment
       * completion.
       */
      percentComplete_: {
        type: Number,
        value: 0,
        observer: 'onProgressChanged_',
      },
    };
  }

  allowAddAnotherFinger: boolean;
  authToken: string;
  private browserProxy_: FingerprintBrowserProxy;
  private percentComplete_: number;
  private problemMessage_: string;
  private step_: FingerprintSetupStep;
  private tapSensorMessageTimeoutId_: number;

  constructor() {
    super();

    /**
     * A message shows after the user has not scanned a finger during setup.
     * This is the set timeout id.
     */
    this.tapSensorMessageTimeoutId_ = 0;

    this.browserProxy_ = FingerprintBrowserProxyImpl.getInstance();
  }


  override connectedCallback(): void {
    super.connectedCallback();

    this.addWebUiListener(
        'on-fingerprint-scan-received', this.onScanReceived_.bind(this));
    this.addWebUiListener('on-screen-locked', this.onScreenLocked_.bind(this));
    this.$.arc.reset();
    this.browserProxy_.startEnroll(this.authToken);
    this.$.dialog.showModal();
  }

  override disconnectedCallback(): void {
    this.cancelCurrentEnroll();
  }

  private cancelCurrentEnroll(): void {
    if (this.step_ !== FingerprintSetupStep.READY) {
      this.browserProxy_.cancelCurrentEnroll();
    }
    // Note: reset_ resets |step_| back to the default, so handle anything that
    // checks |step_| before resetting.
    this.reset_();
  }

  private closeDialog(): void {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  }

  private clearSensorMessageTimeout_(): void {
    if (this.tapSensorMessageTimeoutId_ !== 0) {
      clearTimeout(this.tapSensorMessageTimeoutId_);
      this.tapSensorMessageTimeoutId_ = 0;
    }
  }

  /**
   * Resets the dialog to its start state. Call this when the dialog gets
   * closed.
   */
  private reset_(): void {
    this.step_ = FingerprintSetupStep.LOCATE_SCANNER;
    this.percentComplete_ = 0;
    this.clearSensorMessageTimeout_();
  }

  /**
   * Cancel the current enrollment and closes the dialog.
   * Important to cancel first while we know the current state (step_).
   */
  private onClose_(): void {
    this.cancelCurrentEnroll();
    this.closeDialog();
  }

  /**
   * Advances steps, shows problems and animates the progress as needed based
   * on scan results.
   */
  private onScanReceived_(scan: FingerprintScan): void {
    if (scan.isComplete) {
      this.problemMessage_ = '';
      this.step_ = FingerprintSetupStep.READY;
      this.clearSensorMessageTimeout_();
      const event =
          new CustomEvent('add-fingerprint', {bubbles: true, composed: true});
      this.dispatchEvent(event);
      this.percentComplete_ = scan.percentComplete;
      return;
    }
    switch (this.step_) {
      case FingerprintSetupStep.LOCATE_SCANNER:
        this.$.arc.reset();
        this.step_ = FingerprintSetupStep.MOVE_FINGER;
        this.percentComplete_ = scan.percentComplete;
        this.setProblem_(scan.result);
        break;
      case FingerprintSetupStep.MOVE_FINGER:
        this.setProblem_(scan.result);
        this.percentComplete_ = scan.percentComplete;
        break;
      case FingerprintSetupStep.READY:
        break;
      default:
        assertNotReached();
    }
  }

  /**
   * When the screen is getting locked during enrollment we close
   * the dialog to cancel the enrollment process and make the fingerprint
   * unlock available to the user.
   */
  private onScreenLocked_(screenIsLocked: boolean): void {
    if (screenIsLocked) {
      this.cancelCurrentEnroll();
      this.closeDialog();
    }
  }


  /**
   * Sets the instructions based on which phase of the fingerprint setup we
   * are on.
   * step: The current step the fingerprint setup is on.
   * problemMessage: Message for the scan result.
   */
  private getInstructionMessage_(
      step: FingerprintSetupStep, problemMessage: string): string {
    switch (step) {
      case FingerprintSetupStep.LOCATE_SCANNER:
        return this.i18n('configureFingerprintInstructionLocateScannerStep');
      case FingerprintSetupStep.MOVE_FINGER:
        return problemMessage;
      case FingerprintSetupStep.READY:
        return this.i18n('configureFingerprintInstructionReadyStep');
      default:
        assertNotReached();
    }
  }

  /**
   * Set the problem message based on the result from the fingerprint scanner.
   * scanResult: The result the fingerprint scanner gives.
   */
  private setProblem_(scanResult: FingerprintResultType): void {
    this.clearSensorMessageTimeout_();
    switch (scanResult) {
      case FingerprintResultType.SUCCESS:
        this.problemMessage_ = '';
        this.tapSensorMessageTimeoutId_ = setTimeout(() => {
          this.problemMessage_ = this.i18n('configureFingerprintLiftFinger');
        }, SHOW_TAP_SENSOR_MESSAGE_DELAY_MS);
        break;
      case FingerprintResultType.PARTIAL:
      case FingerprintResultType.INSUFFICIENT:
      case FingerprintResultType.SENSOR_DIRTY:
      case FingerprintResultType.TOO_SLOW:
      case FingerprintResultType.TOO_FAST:
        this.problemMessage_ = this.i18n('configureFingerprintTryAgain');
        break;
      case FingerprintResultType.IMMOBILE:
        this.problemMessage_ = this.i18n('configureFingerprintImmobile');
        break;
      default:
        assertNotReached();
    }
  }

  /**
   * Displays the text of the close button based on which phase of the
   * fingerprint setup we are on.
   * step: The current step the fingerprint setup is on.
   */
  private getCloseButtonText_(step: FingerprintSetupStep): string {
    if (step === FingerprintSetupStep.READY) {
      return this.i18n('done');
    }

    return this.i18n('cancel');
  }

  private getCloseButtonClass_(step: FingerprintSetupStep): string {
    if (step === FingerprintSetupStep.READY) {
      return 'action-button';
    }

    return 'cancel-button';
  }

  private hideAddAnother_(
      step: FingerprintSetupStep, allowAddAnotherFinger: boolean): boolean {
    return step !== FingerprintSetupStep.READY || !allowAddAnotherFinger;
  }

  /**
   * Enrolls the finished fingerprint and sets the dialog back to step one to
   * prepare to enroll another fingerprint.
   */
  private onAddAnotherFingerprint_(): void {
    this.reset_();
    this.$.arc.reset();
    this.step_ = FingerprintSetupStep.MOVE_FINGER;
    this.browserProxy_.startEnroll(this.authToken);
    recordSettingChange(Setting.kAddFingerprintV2);
  }

  /**
   * Whether scanner location should be shown at the current step.
   */
  private showScannerLocation_(): boolean {
    return this.step_ === FingerprintSetupStep.LOCATE_SCANNER;
  }

  /**
   * Whether fingerprint progress circle should be shown at the current step.
   */
  private showArc_(): boolean {
    return this.step_ === FingerprintSetupStep.MOVE_FINGER ||
        this.step_ === FingerprintSetupStep.READY;
  }

  /**
   * Observer for percentComplete_.
   */
  private onProgressChanged_(newValue: number, oldValue: number): void {
    // Start a new enrollment, so reset all enrollment related states.
    if (newValue === 0) {
      this.$.arc.reset();
      return;
    }

    this.$.arc.setProgress(oldValue, newValue, newValue === 100);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsSetupFingerprintDialogElement.is]:
        SettingsSetupFingerprintDialogElement;
  }
}

customElements.define(
    SettingsSetupFingerprintDialogElement.is,
    SettingsSetupFingerprintDialogElement);
