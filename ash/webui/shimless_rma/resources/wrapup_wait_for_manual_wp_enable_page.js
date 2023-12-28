// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared.css.js';
import './base_page.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {HardwareWriteProtectionStateObserverInterface, HardwareWriteProtectionStateObserverReceiver, ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {disableNextButton, enableNextButton, executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';
import {getTemplate} from './wrapup_wait_for_manual_wp_enable_page.html.js';

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
    return getTemplate();
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService = getShimlessRmaService();
    /**
     * Receiver responsible for observing hardware write protection state.
     * @private {
     *  ?HardwareWriteProtectionStateObserverReceiver}
     */
    this.hardwareWriteProtectionStateObserverReceiver =
        new HardwareWriteProtectionStateObserverReceiver(
            /**
             * @type {!HardwareWriteProtectionStateObserverInterface}
             */
            (this));

    this.shimlessRmaService.observeHardwareWriteProtectionState(
        this.hardwareWriteProtectionStateObserverReceiver.$
            .bindNewPipeAndPassRemote());
  }

  /** @override */
  ready() {
    super.ready();

    focusPageTitle(this);
  }

  /**
   * @param {boolean} enabled
   */
  onHardwareWriteProtectionStateChanged(enabled) {
    if (enabled) {
      executeThenTransitionState(
          this, () => this.shimlessRmaService.writeProtectManuallyEnabled());
    }
  }
}

customElements.define(
    WrapupWaitForManualWpEnablePage.is, WrapupWaitForManualWpEnablePage);
