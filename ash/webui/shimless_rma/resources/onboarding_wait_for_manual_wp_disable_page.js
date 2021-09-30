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
      /** @protected */
      hwwpEnabled_: {
        type: Boolean,
        value: true,
      },
    };
  }

  // TODO(gavindodd): battery_status_card.js uses created() and detached() to
  // create and close observer. Is that the pattern that should be used here?

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
    /** @private {?HardwareWriteProtectionStateObserverReceiver} */
    this.hardwareWriteProtectionStateObserverReceiver_ =
        new HardwareWriteProtectionStateObserverReceiver(
            /** @type {!HardwareWriteProtectionStateObserverInterface} */
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

    if(!this.hidden) {
      // TODO(gavindodd): Should this automatically progress to the next state?
      this.dispatchEvent(new CustomEvent(
          'disable-next-button',
          {bubbles: true, composed: true, detail: this.hwwpEnabled_},
          ));
    }
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (!this.hwwpEnabled_) {
      // TODO(crbug.com/1218180): Replace with a state specific function e.g.
      // WriteProtectManuallyDisabled()
      return this.shimlessRmaService_.transitionNextState();
    } else {
      return Promise.reject(
          new Error('Hardware Write Protection is not disabled.'));
    }
  }
};

customElements.define(
    OnboardingWaitForManualWpDisablePageElement.is,
    OnboardingWaitForManualWpDisablePageElement);
