// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {HardwareWriteProtectionStateObserverInterface, HardwareWriteProtectionStateObserverReceiver, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {disableAllButtons, focusPageTitle} from './shimless_rma_util.js';

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

  /** @override */
  ready() {
    super.ready();

    focusPageTitle(this);
  }

  /**
   * @param {boolean} enabled
   * @public
   */
  onHardwareWriteProtectionStateChanged(enabled) {
    this.hwwpEnabled_ = enabled;

    if(!this.hidden) {
      if (!this.hwwpEnabled_) {
        disableAllButtons(this, /*showBusyStateOverlay=*/ false);
        // TODO(swifton): Hide the cancel button.
      }
    }
  }

  /**
   * @return {string}
   * @protected
   */
  getPageTitle_() {
    return this.hwwpEnabled_ ? this.i18n('manuallyDisableWpTitleText') :
                               this.i18n('manuallyDisableWpTitleTextReboot');
  }

  /**
   * @return {string}
   * @protected
   */
  getInstructions_() {
    return this.hwwpEnabled_ ?
        this.i18n('manuallyDisableWpInstructionsText') :
        this.i18n('manuallyDisableWpInstructionsTextReboot');
  }
}

customElements.define(
    OnboardingWaitForManualWpDisablePage.is,
    OnboardingWaitForManualWpDisablePage);
