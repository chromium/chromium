// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import './base_page.js';
import './shimless_rma_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {createCustomEvent, OPEN_LOGS_DIALOG, OpenLogsDialogEvent} from './events.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {PowerCableStateObserverReceiver, ShimlessRmaServiceInterface, ShutdownMethod} from './shimless_rma.mojom-webui.js';
import {executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';
import {getTemplate} from './wrapup_repair_complete_page.html.js';

declare global {
  interface HTMLElementEventMap {
    [OPEN_LOGS_DIALOG]: OpenLogsDialogEvent;
  }
}

/**
 * @fileoverview
 * 'wrapup-repair-complete-page' is the main landing page for the shimless rma
 * process.
 */

const WrapupRepairCompletePageBase = I18nMixin(PolymerElement);

/**
 * Supported options for finishing RMA.
 */
enum FinishRmaOption {
  SHUTDOWN = 'shutdown',
  REBOOT = 'reboot',
}

export class WrapupRepairCompletePage extends WrapupRepairCompletePageBase {
  static get is() {
    return 'wrapup-repair-complete-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.ts.
       */
      allButtonsDisabled: {
        reflectToAttribute: true,
        type: Boolean,
      },

      /**
       * Keeps the shutdown and reboot buttons disabled after the response from
       * the service to prevent successive shutdown or reboot attempts.
       */
      shutdownButtonsDisabled: {
        type: Boolean,
        value: false,
      },

      /**
       * Assume plugged in is true until first observation.
       */
      pluggedIn: {
        reflectToAttribute: true,
        type: Boolean,
        value: true,
      },

      selectedFinishRmaOption: {
        type: String,
        value: '',
      },

      /**
       * This variable needs to remain public because the unit tests need to
       * check its value.
       * TODO(b:315002705): Make this property protected and add a test for it.
       */
      batteryTimeoutID: {
        type: Number,
        value: -1,
      },

      /**
       * This variable needs to remain public because the unit tests need to
       * set it to 0.
       * TODO(b:315002705): Make this property protected and add a test for it.
       */
      batteryTimeoutInMs: {
        type: Number,
        value: 5000,
      },
    };
  }

  allButtonsDisabled: boolean;
  batteryTimeoutID: number;
  batteryTimeoutInMs: number;
  protected shutdownButtonsDisabled: boolean;
  protected pluggedIn: boolean;
  protected selectedFinishRmaOption: string;
  private shimlessRmaService: ShimlessRmaServiceInterface =
      getShimlessRmaService();
  powerCableStateReceiver: PowerCableStateObserverReceiver;

  constructor() {
    super();

    this.powerCableStateReceiver = new PowerCableStateObserverReceiver(this);

    this.shimlessRmaService.observePowerCableState(
        this.powerCableStateReceiver.$.bindNewPipeAndPassRemote());
  }

  override ready() {
    super.ready();

    focusPageTitle(this);
  }

  protected onDiagnosticsButtonClick(): void {
    this.shimlessRmaService.launchDiagnostics();
  }

  protected onShutDownButtonClick(e: Event): void {
    e.preventDefault();
    this.selectedFinishRmaOption = FinishRmaOption.SHUTDOWN;
    this.shimlessRmaService.getPowerwashRequired().then(
        (result: {powerwashRequired: boolean}) => {
          this.handlePowerwash(result.powerwashRequired);
        });
  }

  /**
   * Handles the response to getPowerwashRequired from the backend.
   */
  private handlePowerwash(powerwashRequired: boolean): void {
    if (powerwashRequired) {
      const dialog: CrDialogElement|null =
          this.shadowRoot!.querySelector('#powerwashDialog');
      assert(dialog);
      if (!dialog.open) {
        dialog.showModal();
      }
    } else {
      this.shutDownOrReboot();
    }
  }

  private shutDownOrReboot(): void {
    // Keeps the buttons disabled until the device is shutdown.
    this.shutdownButtonsDisabled = true;

    if (this.selectedFinishRmaOption === FinishRmaOption.SHUTDOWN) {
      this.endRmaAndShutdown();
    } else {
      this.endRmaAndReboot();
    }
  }

  /**
   * Sends a shutdown request to the backend.
   */
  private endRmaAndShutdown(): void {
    executeThenTransitionState(
        this, () => this.shimlessRmaService.endRma(ShutdownMethod.kShutdown));
  }

  protected getPowerwashDescriptionString(): string {
    return this.selectedFinishRmaOption === FinishRmaOption.SHUTDOWN ?
        this.i18n('powerwashDialogShutdownDescription') :
        this.i18n('powerwashDialogRebootDescription');
  }

  protected onPowerwashButtonClick(e: Event): void {
    e.preventDefault();
    const dialog: CrDialogElement|null =
        this.shadowRoot!.querySelector('#powerwashDialog');
    assert(dialog);
    dialog.close();
    this.shutDownOrReboot();
  }

  protected onRebootButtonClick(e: Event): void {
    e.preventDefault();
    this.selectedFinishRmaOption = FinishRmaOption.REBOOT;
    this.shimlessRmaService.getPowerwashRequired().then(
        (result: {powerwashRequired: boolean}) => {
          this.handlePowerwash(result.powerwashRequired);
        });
  }

  /**
   * Sends a reboot request to the backend.
   */
  private endRmaAndReboot(): void {
    executeThenTransitionState(
        this, () => this.shimlessRmaService.endRma(ShutdownMethod.kReboot));
  }

  protected onRmaLogButtonClick(): void {
    this.dispatchEvent(createCustomEvent(OPEN_LOGS_DIALOG, {}));
  }

  protected onBatteryCutButtonClick(): void {
    const dialog: CrDialogElement|null =
        this.shadowRoot!.querySelector('#batteryCutoffDialog');
    assert(dialog);
    if (!dialog.open) {
      dialog.showModal();
    }

    // This is necessary because after the timeout "this" will be the window,
    // and not WrapupRepairCompletePage.
    const cutoffBattery = function(wrapupRepairCompletePage: HTMLElement|
                                   null): void {
      assert(wrapupRepairCompletePage);
      const dialog: CrDialogElement|null =
          wrapupRepairCompletePage.shadowRoot!.querySelector(
              '#batteryCutoffDialog');
      assert(dialog);
      dialog.close();
      executeThenTransitionState(
          wrapupRepairCompletePage,
          () => (wrapupRepairCompletePage as HTMLElement & {
                  shimlessRmaService: ShimlessRmaServiceInterface,
                }).shimlessRmaService.endRma(ShutdownMethod.kBatteryCutoff));
    };

    if (this.batteryTimeoutID === -1) {
      this.batteryTimeoutID =
          setTimeout(() => cutoffBattery(this), this.batteryTimeoutInMs);
    }
  }

  private cutoffBattery(): void {
    const dialog: CrDialogElement|null =
        this.shadowRoot!.querySelector('#batteryCutoffDialog');
    assert(dialog);
    dialog.close();
    executeThenTransitionState(
        this,
        () => this.shimlessRmaService.endRma(ShutdownMethod.kBatteryCutoff));
  }

  protected onCutoffShutdownButtonClick(): void {
    this.cutoffBattery();
  }

  protected closePowerwashDialog(): void {
    const dialog: CrDialogElement|null =
        this.shadowRoot!.querySelector('#powerwashDialog');
    assert(dialog);
    dialog.close();
  }

  protected onCutoffCancelClick(): void {
    this.cancelBatteryCutoff();
  }

  private cancelBatteryCutoff(): void {
    const batteryCutoffDialog: CrDialogElement|null =
        this.shadowRoot!.querySelector('#batteryCutoffDialog');
    assert(batteryCutoffDialog);
    batteryCutoffDialog.close();

    if (this.batteryTimeoutID !== -1) {
      clearTimeout(this.batteryTimeoutID);
      this.batteryTimeoutID = -1;
    }
  }

  /**
   * Implements PowerCableStateObserver.onPowerCableStateChanged()
   */
  onPowerCableStateChanged(pluggedIn: boolean): void {
    this.pluggedIn = pluggedIn;

    if (this.pluggedIn) {
      this.cancelBatteryCutoff();
    }

    const icon: HTMLElement|null =
        this.shadowRoot!.querySelector('#batteryCutoffIcon');
    assert(icon);
    icon.setAttribute(
        'icon',
        this.pluggedIn ? 'shimless-icon:battery-cutoff-disabled' :
                         'shimless-icon:battery-cutoff');
  }

  protected disableBatteryCutButton(): boolean {
    return this.pluggedIn || this.allButtonsDisabled;
  }

  protected getDiagnosticsIcon(): string {
    return this.allButtonsDisabled ? 'shimless-icon:diagnostics-disabled' :
                                     'shimless-icon:diagnostics';
  }

  protected getRmaLogIcon(): string {
    return this.allButtonsDisabled ? 'shimless-icon:rma-log-disabled' :
                                     'shimless-icon:rma-log';
  }

  protected getBatteryCutoffIcon(): string {
    return this.allButtonsDisabled ? 'shimless-icon:battery-cutoff-disabled' :
                                     'shimless-icon:battery-cutoff';
  }

  protected disableShutdownButtons(): boolean {
    return this.shutdownButtonsDisabled || this.allButtonsDisabled;
  }

  protected getRepairCompletedShutoffText(): string {
    return this.pluggedIn ?
        this.i18n('repairCompletedShutoffInstructionsText') :
        this.i18n('repairCompletedShutoffDescriptionText');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [WrapupRepairCompletePage.is]: WrapupRepairCompletePage;
  }
}

customElements.define(WrapupRepairCompletePage.is, WrapupRepairCompletePage);
