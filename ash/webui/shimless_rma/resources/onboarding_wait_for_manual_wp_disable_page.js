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
 * 'onboarding-wait-for-manual-wp-disable-page' wait for the manual HWWP disable
 * to be completed.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OnboardingWaitForManualWpDisablePageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class OnboardingWaitForManualWpDisablePage extends
    OnboardingWaitForManualWpDisablePageBase {
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
   * @public
   * @param {boolean} enabled
   */
  onHardwareWriteProtectionStateChanged(enabled) {
    this.hwwpEnabled_ = enabled;

    if(!this.hidden) {
      // TODO(gavindodd): Should this automatically progress to the next state?
      if (this.hwwpEnabled_) {
        disableNextButton(this);
      } else {
        enableNextButton(this);
      }
    }
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (!this.hwwpEnabled_) {
      return this.shimlessRmaService_.writeProtectManuallyDisabled();
    } else {
      return Promise.reject(
          new Error('Hardware Write Protection is not disabled.'));
    }
  }
}

customElements.define(
    OnboardingWaitForManualWpDisablePage.is,
    OnboardingWaitForManualWpDisablePage);
