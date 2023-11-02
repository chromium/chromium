// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import './base_page.js';
import './shimless_rma_fonts_css.js';
import './shimless_rma_shared_css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {PowerCableStateObserverInterface, PowerCableStateObserverReceiver, RmadErrorCode, ShimlessRmaServiceInterface, ShutdownMethod} from './shimless_rma_types.js';
import {executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';

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
    return html`{__html_template__}`;
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
      shutdownButtonsDisabled_: {
        type: Boolean,
        value: false,
      },

      /**
       * @protected
       * Assume plugged in is true until first observation.
       */
      pluggedIn_: {
        reflectToAttribute: true,
        type: Boolean,
        value: true,
      },

      /** @protected */
      selectedFinishRmaOption_: {
        type: String,
        value: '',
      },

      /**
       * This variable needs to remain public because the unit tests need to
       * check its value.
       */
      batteryTimeoutID_: {
        type: Number,
        value: -1,
      },

      /**
       * This variable needs to remain public because the unit tests need to
       * set it to 0.
       */
      batteryTimeoutInMs_: {
        type: Number,
        value: 5000,
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();

    /** @private {!PowerCableStateObserverReceiver} */
    this.powerCableStateReceiver_ = new PowerCableStateObserverReceiver(
        /** @type {!PowerCableStateObserverInterface} */ (this));

    this.shimlessRmaService_.observePowerCableState(
        this.powerCableStateReceiver_.$.bindNewPipeAndPassRemote());
  }

  /** @override */
  ready() {
    super.ready();

    focusPageTitle(this);
  }

  /** @protected */
  onDiagnosticsButtonClick_() {
    this.shimlessRmaService_.launchDiagnostics();
  }

  /** @protected */
  onShutDownButtonClick_(e) {
    e.preventDefault();
    this.selectedFinishRmaOption_ = FinishRmaOption.SHUTDOWN;
    this.shimlessRmaService_.getPowerwashRequired().then((result) => {
      this.handlePowerwash_(result.powerwashRequired);
    });
  }

  /**
   * Handles the response to getPowerwashRequired from the backend.
   * @private
   */
  handlePowerwash_(powerwashRequired) {
    if (powerwashRequired) {
      const dialog = /** @type {!CrDialogElement} */ (
          this.shadowRoot.querySelector('#powerwashDialog'));
      if (!dialog.open) {
        dialog.showModal();
      }
    } else {
      this.shutDownOrReboot_();
    }
  }

  /** @private */
  shutDownOrReboot_() {
    // Keeps the buttons disabled until the device is shutdown.
    this.shutdownButtonsDisabled_ = true;

    if (this.selectedFinishRmaOption_ === FinishRmaOption.SHUTDOWN) {
      this.endRmaAndShutdown_();
    } else {
      this.endRmaAndReboot_();
    }
  }

  /**
   * Sends a shutdown request to the backend.
   * @private
   */
  endRmaAndShutdown_() {
    executeThenTransitionState(
        this, () => this.shimlessRmaService_.endRma(ShutdownMethod.kShutdown));
  }

  /**
   * @return {string}
   * @protected
   */
  getPowerwashDescriptionString_() {
    return this.selectedFinishRmaOption_ === FinishRmaOption.SHUTDOWN ?
        this.i18n('powerwashDialogShutdownDescription') :
        this.i18n('powerwashDialogRebootDescription');
  }

  /** @protected */
  onPowerwashButtonClick_(e) {
    e.preventDefault();
    const dialog = /** @type {!CrDialogElement} */ (
      this.shadowRoot.querySelector('#powerwashDialog'));
    dialog.close();
    this.shutDownOrReboot_();
  }

  /** @protected */
  onRebootButtonClick_(e) {
    e.preventDefault();
    this.selectedFinishRmaOption_ = FinishRmaOption.REBOOT;
    this.shimlessRmaService_.getPowerwashRequired().then((result) => {
      this.handlePowerwash_(result.powerwashRequired);
    });
  }

  /**
   * Sends a reboot request to the backend.
   * @private
   */
  endRmaAndReboot_() {
    executeThenTransitionState(
        this, () => this.shimlessRmaService_.endRma(ShutdownMethod.kReboot));
  }

  /** @protected */
  onRmaLogButtonClick_() {
    this.dispatchEvent(new CustomEvent('open-logs-dialog', {
      bubbles: true,
      composed: true,
    }));
  }

  /** @protected */
  onBatteryCutButtonClick_() {
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
          () => wrapupRepairCompletePage.shimlessRmaService_.endRma(
              ShutdownMethod.kBatteryCutoff));
    };

    if (this.batteryTimeoutID_ === -1) {
      this.batteryTimeoutID_ =
          setTimeout(() => cutoffBattery(this), this.batteryTimeoutInMs_);
    }
  }

  /** @private */
  cutoffBattery_() {
    this.shadowRoot.querySelector('#batteryCutoffDialog').close();
    executeThenTransitionState(
        this,
        () => this.shimlessRmaService_.endRma(ShutdownMethod.kBatteryCutoff));
  }

  /** @protected */
  onCutoffShutdownButtonClick_() {
    this.cutoffBattery_();
  }

  /** @protected */
  closePowerwashDialog_() {
    this.shadowRoot.querySelector('#powerwashDialog').close();
  }

  /** @protected */
  onCutoffCancelClick_() {
    this.cancelBatteryCutoff_();
  }

  /** @private */
  cancelBatteryCutoff_() {
    const batteryCutoffDialog = /** @type {!CrDialogElement} */ (
        this.shadowRoot.querySelector('#batteryCutoffDialog'));
    batteryCutoffDialog.close();

    if (this.batteryTimeoutID_ !== -1) {
      clearTimeout(this.batteryTimeoutID_);
      this.batteryTimeoutID_ = -1;
    }
  }

  /**
   * Implements PowerCableStateObserver.onPowerCableStateChanged()
   * @param {boolean} pluggedIn
   */
  onPowerCableStateChanged(pluggedIn) {
    this.pluggedIn_ = pluggedIn;

    if (this.pluggedIn_) {
      this.cancelBatteryCutoff_();
    }

    const icon = /** @type {!HTMLElement}*/ (
        this.shadowRoot.querySelector('#batteryCutoffIcon'));
    icon.setAttribute(
        'icon',
        this.pluggedIn_ ? 'shimless-icon:battery-cutoff-disabled' :
                          'shimless-icon:battery-cutoff');
  }

  /**
   * @return {boolean}
   * @protected
   */
  disableBatteryCutButton_() {
    return this.pluggedIn_ || this.allButtonsDisabled;
  }

  /**
   * @return {string}
   * @protected
   */
  getDiagnosticsIcon_() {
    return this.allButtonsDisabled ? 'shimless-icon:diagnostics-disabled' :
                                     'shimless-icon:diagnostics';
  }

  /**
   * @return {string}
   * @protected
   */
  getRmaLogIcon_() {
    return this.allButtonsDisabled ? 'shimless-icon:rma-log-disabled' :
                                     'shimless-icon:rma-log';
  }

  /**
   * @return {string}
   * @protected
   */
  getBatteryCutoffIcon_() {
    return this.allButtonsDisabled ? 'shimless-icon:battery-cutoff-disabled' :
                                     'shimless-icon:battery-cutoff';
  }

  /**
   * @return {boolean}
   * @protected
   */
  disableShutdownButtons_() {
    return this.shutdownButtonsDisabled_ || this.allButtonsDisabled;
  }

  /**
   * @return {string}
   * @protected
   */
  getRepairCompletedShutoffText_() {
    return this.pluggedIn_ ?
        this.i18n('repairCompletedShutoffInstructionsText') :
        this.i18n('repairCompletedShutoffDescriptionText');
  }
}

customElements.define(WrapupRepairCompletePage.is, WrapupRepairCompletePage);
