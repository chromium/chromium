// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import './base_page.js';
import './shimless_rma_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {PowerCableStateObserverInterface, PowerCableStateObserverReceiver, RmadErrorCode, ShimlessRmaServiceInterface, ShutdownMethod} from './shimless_rma.mojom-webui.js';
import {executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';
import {getTemplate} from './wrapup_repair_complete_page.html.js';

/**
 * @fileoverview
 * 'wrapup-repair-complete-page' is the main landing page for the shimless rma
 * process.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const WrapupRepairCompletePageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/**
 * Supported options for finishing RMA.
 * @enum {string}
 */
const FinishRmaOption = {
  SHUTDOWN: 'shutdown',
  REBOOT: 'reboot',
};

/** @polymer */
export class WrapupRepairCompletePage extends WrapupRepairCompletePageBase {
  static get is() {
    return 'wrapup-repair-complete-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.js.
       * @type {boolean}
       */
      allButtonsDisabled: {
        reflectToAttribute: true,
        type: Boolean,
      },

      /**
       * Keeps the shutdown and reboot buttons disabled after the response from
       * the service to prevent successive shutdown or reboot attempts.
       * @protected {boolean}
       */
      shutdownButtonsDisabled: {
        type: Boolean,
        value: false,
      },

      /**
       * @protected
       * Assume plugged in is true until first observation.
       */
      pluggedIn: {
        reflectToAttribute: true,
        type: Boolean,
        value: true,
      },

      /** @protected */
      selectedFinishRmaOption: {
        type: String,
        value: '',
      },

      /**
       * This variable needs to remain public because the unit tests need to
       * check its value.
       */
      batteryTimeoutID: {
        type: Number,
        value: -1,
      },

      /**
       * This variable needs to remain public because the unit tests need to
       * set it to 0.
       */
      batteryTimeoutInMs: {
        type: Number,
        value: 5000,
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService = getShimlessRmaService();

    /** @private {!PowerCableStateObserverReceiver} */
    this.powerCableStateReceiver = new PowerCableStateObserverReceiver(
        /** @type {!PowerCableStateObserverInterface} */ (this));

    this.shimlessRmaService.observePowerCableState(
        this.powerCableStateReceiver.$.bindNewPipeAndPassRemote());
  }

  /** @override */
  ready() {
    super.ready();

    focusPageTitle(this);
  }

  /** @protected */
  onDiagnosticsButtonClick() {
    this.shimlessRmaService.launchDiagnostics();
  }

  /** @protected */
  onShutDownButtonClick(e) {
    e.preventDefault();
    this.selectedFinishRmaOption = FinishRmaOption.SHUTDOWN;
    this.shimlessRmaService.getPowerwashRequired().then((result) => {
      this.handlePowerwash(result.powerwashRequired);
    });
  }

  /**
   * Handles the response to getPowerwashRequired from the backend.
   * @private
   */
  handlePowerwash(powerwashRequired) {
    if (powerwashRequired) {
      const dialog = /** @type {!CrDialogElement} */ (
          this.shadowRoot.querySelector('#powerwashDialog'));
      if (!dialog.open) {
        dialog.showModal();
      }
    } else {
      this.shutDownOrReboot();
    }
  }

  /** @private */
  shutDownOrReboot() {
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
   * @private
   */
  endRmaAndShutdown() {
    executeThenTransitionState(
        this, () => this.shimlessRmaService.endRma(ShutdownMethod.kShutdown));
  }

  /**
   * @return {string}
   * @protected
   */
  getPowerwashDescriptionString() {
    return this.selectedFinishRmaOption === FinishRmaOption.SHUTDOWN ?
        this.i18n('powerwashDialogShutdownDescription') :
        this.i18n('powerwashDialogRebootDescription');
  }

  /** @protected */
  onPowerwashButtonClick(e) {
    e.preventDefault();
    const dialog = /** @type {!CrDialogElement} */ (
      this.shadowRoot.querySelector('#powerwashDialog'));
    dialog.close();
    this.shutDownOrReboot();
  }

  /** @protected */
  onRebootButtonClick(e) {
    e.preventDefault();
    this.selectedFinishRmaOption = FinishRmaOption.REBOOT;
    this.shimlessRmaService.getPowerwashRequired().then((result) => {
      this.handlePowerwash(result.powerwashRequired);
    });
  }

  /**
   * Sends a reboot request to the backend.
   * @private
   */
  endRmaAndReboot() {
    executeThenTransitionState(
        this, () => this.shimlessRmaService.endRma(ShutdownMethod.kReboot));
  }

  /** @protected */
  onRmaLogButtonClick() {
    this.dispatchEvent(new CustomEvent('open-logs-dialog', {
      bubbles: true,
      composed: true,
    }));
  }

  /** @protected */
  onBatteryCutButtonClick() {
    const dialog = /** @type {!CrDialogElement} */ (
        this.shadowRoot.querySelector('#batteryCutoffDialog'));
    if (!dialog.open) {
      dialog.showModal();
    }

    // This is necessary because after the timeout "this" will be the window,
    // and not WrapupRepairCompletePage.
    const cutoffBattery = function(wrapupRepairCompletePage) {
      wrapupRepairCompletePage.shadowRoot.querySelector('#batteryCutoffDialog')
          .close();
      executeThenTransitionState(
          wrapupRepairCompletePage,
          () => wrapupRepairCompletePage.shimlessRmaService.endRma(
              ShutdownMethod.kBatteryCutoff));
    };

    if (this.batteryTimeoutID === -1) {
      this.batteryTimeoutID =
          setTimeout(() => cutoffBattery(this), this.batteryTimeoutInMs);
    }
  }

  /** @private */
  cutoffBattery() {
    this.shadowRoot.querySelector('#batteryCutoffDialog').close();
    executeThenTransitionState(
        this,
        () => this.shimlessRmaService.endRma(ShutdownMethod.kBatteryCutoff));
  }

  /** @protected */
  onCutoffShutdownButtonClick() {
    this.cutoffBattery();
  }

  /** @protected */
  closePowerwashDialog() {
    this.shadowRoot.querySelector('#powerwashDialog').close();
  }

  /** @protected */
  onCutoffCancelClick() {
    this.cancelBatteryCutoff();
  }

  /** @private */
  cancelBatteryCutoff() {
    const batteryCutoffDialog = /** @type {!CrDialogElement} */ (
        this.shadowRoot.querySelector('#batteryCutoffDialog'));
    batteryCutoffDialog.close();

    if (this.batteryTimeoutID !== -1) {
      clearTimeout(this.batteryTimeoutID);
      this.batteryTimeoutID = -1;
    }
  }

  /**
   * Implements PowerCableStateObserver.onPowerCableStateChanged()
   * @param {boolean} pluggedIn
   */
  onPowerCableStateChanged(pluggedIn) {
    this.pluggedIn = pluggedIn;

    if (this.pluggedIn) {
      this.cancelBatteryCutoff();
    }

    const icon = /** @type {!HTMLElement}*/ (
        this.shadowRoot.querySelector('#batteryCutoffIcon'));
    icon.setAttribute(
        'icon',
        this.pluggedIn ? 'shimless-icon:battery-cutoff-disabled' :
                         'shimless-icon:battery-cutoff');
  }

  /**
   * @return {boolean}
   * @protected
   */
  disableBatteryCutButton() {
    return this.pluggedIn || this.allButtonsDisabled;
  }

  /**
   * @return {string}
   * @protected
   */
  getDiagnosticsIcon() {
    return this.allButtonsDisabled ? 'shimless-icon:diagnostics-disabled' :
                                     'shimless-icon:diagnostics';
  }

  /**
   * @return {string}
   * @protected
   */
  getRmaLogIcon() {
    return this.allButtonsDisabled ? 'shimless-icon:rma-log-disabled' :
                                     'shimless-icon:rma-log';
  }

  /**
   * @return {string}
   * @protected
   */
  getBatteryCutoffIcon() {
    return this.allButtonsDisabled ? 'shimless-icon:battery-cutoff-disabled' :
                                     'shimless-icon:battery-cutoff';
  }

  /**
   * @return {boolean}
   * @protected
   */
  disableShutdownButtons() {
    return this.shutdownButtonsDisabled || this.allButtonsDisabled;
  }

  /**
   * @return {string}
   * @protected
   */
  getRepairCompletedShutoffText() {
    return this.pluggedIn ?
        this.i18n('repairCompletedShutoffInstructionsText') :
        this.i18n('repairCompletedShutoffDescriptionText');
  }
}

customElements.define(WrapupRepairCompletePage.is, WrapupRepairCompletePage);
