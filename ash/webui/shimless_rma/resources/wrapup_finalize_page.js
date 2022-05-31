// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './shimless_rma_shared_css.js';
import './base_page.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {FinalizationError, FinalizationObserverInterface, FinalizationObserverReceiver, FinalizationStatus, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {executeThenTransitionState} from './shimless_rma_util.js';

/** @type {!Object<!FinalizationStatus, string>} */
const finalizationStatusTextKeys = {
  [FinalizationStatus.kInProgress]: 'finalizePageProgressText',
  [FinalizationStatus.kComplete]: 'finalizePageCompleteText',
  [FinalizationStatus.kFailedBlocking]: 'finalizePageFailedBlockingText',
  [FinalizationStatus.kFailedNonBlocking]: 'finalizePageFailedNonBlockingText',
};

/**
 * @fileoverview
 * 'wrapup-finalize-page' wait for device finalization and hardware verification
 * to be completed.
 */

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

      /** @protected {boolean} */
      shouldShowSpinner_: {
        type: Boolean,
        value: false,
      },

      /** @protected {boolean} */
      shouldShowRetryButton_: {
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
     * @private {?FinalizationObserverReceiver}
     */
    this.finalizationObserverReceiver_ = new FinalizationObserverReceiver(
        /** @type {!FinalizationObserverInterface} */ (this));

    this.shimlessRmaService_.observeFinalizationStatus(
        this.finalizationObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /**
   * @param {!FinalizationStatus} status
   * @param {number} progress
   * @param {!FinalizationError} error
   */
  onFinalizationUpdated(status, progress, error) {
    const isErrorStatus = status === FinalizationStatus.kFailedBlocking ||
        status === FinalizationStatus.kFailedNonBlocking;
    const isHardwareWpError =
        isErrorStatus && error === FinalizationError.kCannotEnableHardwareWp;

    this.finalizationMessage_ = this.i18n(
        isHardwareWpError ? 'finalizePageProgressText' :
                            finalizationStatusTextKeys[status]);

    if (status === FinalizationStatus.kComplete) {
      executeThenTransitionState(
          this, () => this.shimlessRmaService_.finalizationComplete());
      return;
    }

    this.shouldShowSpinner_ =
        isHardwareWpError || status === FinalizationStatus.kInProgress;

    if (isHardwareWpError) {
      const dialog = /** @type {!CrDialogElement} */ (
          this.shadowRoot.querySelector('#hardwareWpDisabledDialog'));
      dialog.showModal();
    } else {
      // For other errors, continue using retry button to handle for now.
      this.shouldShowRetryButton_ = isErrorStatus;
    }
  }

  /** @protected */
  onRetryFinalizationButtonClicked_() {
    if (!this.shouldShowRetryButton_) {
      console.error('Finalization has not failed.');
      return;
    }

    executeThenTransitionState(
        this, () => this.shimlessRmaService_.retryFinalization());
  }

  /**
   * Handles the try again button on hardwareWpDisabledDialog is clicked.
   * @protected
   */
  onTryAgainButtonClick_() {
    const dialog = /** @type {!CrDialogElement} */ (
        this.shadowRoot.querySelector('#hardwareWpDisabledDialog'));
    dialog.close();

    executeThenTransitionState(
        this, () => this.shimlessRmaService_.retryFinalization());
  }
}

customElements.define(WrapupFinalizePage.is, WrapupFinalizePage);
