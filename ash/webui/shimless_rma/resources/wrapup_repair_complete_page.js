// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import './base_page.js';
import './shimless_rma_fonts_css.js';
import './shimless_rma_shared_css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {PowerCableStateObserverInterface, PowerCableStateObserverReceiver, ShimlessRmaServiceInterface} from './shimless_rma_types.js';

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
      allButtonsDisabled: Boolean,

      /** @protected */
      log_: {
        type: String,
        value: '',
      },

      /**
       * @protected
       * Assume plugged in is true until first observation.
       */
      pluggedIn_: {
        reflectToAttribute: true,
        type: Boolean,
        value: true,
      }
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

  /** @protected */
  onDiagnosticsButtonClick_() {
    this.shimlessRmaService_.launchDiagnostics();
  }

  /** @protected */
  onShutDownButtonClick_(e) {
    e.preventDefault();
    this.dispatchEvent(new CustomEvent(
        'transition-state',
        {
          bubbles: true,
          composed: true,
          detail: (() => {
            return this.shimlessRmaService_.endRmaAndShutdown();
          })
        },
        ));
  }

  /** @protected */
  onRebootButtonClick_(e) {
    e.preventDefault();
    this.dispatchEvent(new CustomEvent(
        'transition-state',
        {
          bubbles: true,
          composed: true,
          detail: (() => {
            return this.shimlessRmaService_.endRmaAndReboot();
          })
        },
        ));
  }

  /** @protected */
  onRmaLogButtonClick_() {
    this.shimlessRmaService_.getLog().then((res) => this.log_ = res.log);
    const dialog = /** @type {!CrDialogElement} */ (
        this.shadowRoot.querySelector('#logsDialog'));
    if (!dialog.open) {
      dialog.showModal();
    }
  }

  /** @protected */
  onBatteryCutButtonClick_() {
    this.dispatchEvent(new CustomEvent(
        'transition-state',
        {
          bubbles: true,
          composed: true,
          detail: (() => {
            return this.shimlessRmaService_.endRmaAndCutoffBattery();
          })
        },
        ));
  }

  /** @protected */
  onCancelClick_() {
    const dialogs = /** @type {!NodeList<!CrDialogElement>} */ (
        this.shadowRoot.querySelectorAll('cr-dialog'));
    Array.from(dialogs).map((dialog) => {
      dialog.close();
    });
  }

  /**
   * Implements PowerCableStateObserver.onPowerCableStateChanged()
   * @param {boolean} pluggedIn
   */
  onPowerCableStateChanged(pluggedIn) {
    this.pluggedIn_ = pluggedIn;

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
}

customElements.define(WrapupRepairCompletePage.is, WrapupRepairCompletePage);
