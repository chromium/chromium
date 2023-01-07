// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './shimless_rma_shared_css.js';
import './base_page.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {FinalizationError, FinalizationObserverInterface, FinalizationObserverReceiver, FinalizationStatus, RmadErrorCode, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';

/** @type {!Object<!FinalizationStatus, string>} */
const finalizationStatusTextKeys = {
  [FinalizationStatus.kInProgress]: 'finalizePageProgressText',
  [FinalizationStatus.kComplete]: 'finalizePageCompleteText',
};

/**
 * @fileoverview
 * 'wrapup-finalize-page' wait for device finalization and hardware verification
 * to be completed.
 */

/**
 * The prefix for a `FinalizationError` displayed on the Hardware Error page.
 * @type {number}
 */
export const FINALIZATION_ERROR_CODE_PREFIX = 2000;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const WrapupFinalizePageBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class WrapupFinalizePage extends WrapupFinalizePageBase {
  static get is() {
    return 'wrapup-finalize-page';
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
      finalizationMessage_: {
        type: String,
        value: '',
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
    /**
     * Receiver responsible for observing finalization progress and state.
     * @private {?FinalizationObserverReceiver}
     */
    this.finalizationObserverReceiver_ = new FinalizationObserverReceiver(
        /** @type {!FinalizationObserverInterface} */ (this));

    this.shimlessRmaService_.observeFinalizationStatus(
        this.finalizationObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /** @override */
  ready() {
    super.ready();

    focusPageTitle(this);
  }

  /**
   * @param {!FinalizationStatus} status
   * @param {number} progress
   * @param {!FinalizationError} error
   */
  onFinalizationUpdated(status, progress, error) {
    if (status === FinalizationStatus.kFailedBlocking ||
        status === FinalizationStatus.kFailedNonBlocking) {
      this.dispatchEvent(new CustomEvent('fatal-hardware-error', {
        bubbles: true,
        composed: true,
        detail: {
          rmadErrorCode: RmadErrorCode.kFinalizationFailed,
          fatalErrorCode: (FINALIZATION_ERROR_CODE_PREFIX + error),
        },
      }));
    } else {
      this.finalizationMessage_ = this.i18n(finalizationStatusTextKeys[status]);

      if (status === FinalizationStatus.kComplete) {
        executeThenTransitionState(
            this, () => this.shimlessRmaService_.finalizationComplete());
        return;
      }
    }
  }
}

customElements.define(WrapupFinalizePage.is, WrapupFinalizePage);
