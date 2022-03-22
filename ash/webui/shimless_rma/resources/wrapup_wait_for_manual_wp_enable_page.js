// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {HardwareWriteProtectionStateObserverInterface, HardwareWriteProtectionStateObserverReceiver, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {disableNextButton, enableNextButton} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'wrapup-wait-for-manual-wp-enable-page' wait for the manual HWWP enable to be
 * completed.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const WrapupWaitForManualWpEnablePageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class WrapupWaitForManualWpEnablePage extends
    WrapupWaitForManualWpEnablePageBase {
  static get is() {
    return 'wrapup-wait-for-manual-wp-enable-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @protected */
      hwwpEnabled_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
    /**
     * Receiver responsible for observing hardware write protection state.
     * @private {
     *  ?HardwareWriteProtectionStateObserverReceiver}
     */
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
    return this.hwwpEnabled_ ? this.i18n('manuallyEnabledWpMessageText') :
                               this.i18n('manuallyEnableWpInstructionsText');
  }

  /**
   * @param {boolean} enabled
   */
  onHardwareWriteProtectionStateChanged(enabled) {
    this.hwwpEnabled_ = enabled;
    // TODO(gavindodd): enable/disable next button. Or should it automatically
    // progress to the next state?
    if (this.hwwpEnabled_) {
      enableNextButton(this);
    } else {
      disableNextButton(this);
    }
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (this.hwwpEnabled_) {
      return this.shimlessRmaService_.writeProtectManuallyEnabled();
    } else {
      return Promise.reject(
          new Error('Hardware Write Protection is not enabled.'));
    }
  }
}

customElements.define(
    WrapupWaitForManualWpEnablePage.is, WrapupWaitForManualWpEnablePage);
