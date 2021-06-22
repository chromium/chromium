// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {HardwareWriteProtectionStateObserverInterface, HardwareWriteProtectionStateObserverReceiver, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

// TODO(gavindodd): Update text for i18n
const openDeviceMessage = 'Open your device and disconnect the battery.';
const hwwpDisabledMessage = 'HWWP disabled.';

/**
 * @fileoverview
 * 'onboarding-wait-for-manual-wp-disable-page' wait for the manual HWWP disable
 * to be completed.
 */
export class OnboardingWaitForManualWpDisablePageElement extends
    PolymerElement {
  static get is() {
    return 'onboarding-wait-for-manual-wp-disable-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {ShimlessRmaServiceInterface} */
      shimlessRmaService_: {
        type: Object,
        value: {},
      },

      /** @protected {boolean} */
      hwwpEnabled_: {
        type: Boolean,
        value: true,
      },

      /**
       * Receiver responsible for observing hardware write protection state.
       * @private {
       *  ?HardwareWriteProtectionStateObserverReceiver}
       */
      hardwareWriteProtectionStateObserverReceiver_: {
        type: Object,
        value: null,
      },

    };
  }

  // TODO: battery_status_card.js uses created() and detached() to create and
  // close observer.

  /** @override */
  ready() {
    super.ready();
    this.shimlessRmaService_ = getShimlessRmaService();
    this.observeHardwareWriteProtectionState_();
  }

  /** @private */
  observeHardwareWriteProtectionState_() {
    this.hardwareWriteProtectionStateObserverReceiver_ =
        new HardwareWriteProtectionStateObserverReceiver(
            /**
             * @type {!HardwareWriteProtectionStateObserverInterface}
             */
            (this));

    this.shimlessRmaService_.observeHardwareWriteProtectionState(
        this.hardwareWriteProtectionStateObserverReceiver_.$
            .bindNewPipeAndPassRemote());
  }

  /**
   * @protected
   * @param {boolean} hwwpEnabled
   * @return {string}
   */
  getBodyText_(hwwpEnabled) {
    return this.hwwpEnabled_ ? openDeviceMessage : hwwpDisabledMessage;
  }

  /**
   * @public
   * @param {boolean} enabled
   */
  onHardwareWriteProtectionStateChanged(enabled) {
    this.hwwpEnabled_ = enabled;
    // TODO(gavindodd): enable/disable next button. Or should it automatically
    // progress to the next state?
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (!this.hwwpEnabled_) {
      // TODO(crbug.com/1218180): Replace with a state specific function e.g.
      // WriteProtectManuallyDisabled()
      return this.shimlessRmaService_.getNextState();
    } else {
      return Promise.reject(
          new Error('Hardware Write Protection is not disabled.'));
    }
  }
};

customElements.define(
    OnboardingWaitForManualWpDisablePageElement.is,
    OnboardingWaitForManualWpDisablePageElement);
