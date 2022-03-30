// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shimless_rma_shared_css.js';
import './base_page.js';
import './icons.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ProvisioningObserverInterface, ProvisioningObserverReceiver, ProvisioningStatus, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {disableNextButton, enableNextButton, executeThenTransitionState} from './shimless_rma_util.js';

/** @type {!Object<!ProvisioningStatus, string>} */
const provisioningStatusTextKeys = {
  [ProvisioningStatus.kInProgress]: 'provisioningPageInProgressText',
  [ProvisioningStatus.kComplete]: 'provisioningPageCompleteText',
  [ProvisioningStatus.kFailedBlocking]: 'provisioningPageFailedBlockingText',
  [ProvisioningStatus.kFailedNonBlocking]:
      'provisioningPageFailedNonBlockingText',
};

/**
 * @fileoverview
 * 'reimaging-provisioning-page' provisions the device then auto-transitions to
 * the next page once complete.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ReimagingProvisioningPageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ReimagingProvisioningPage extends ReimagingProvisioningPageBase {
  static get is() {
    return 'reimaging-provisioning-page';
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

      /** @protected {!ProvisioningStatus} */
      status_: {
        type: Object,
      },

      /** @protected */
      statusString_: {
        type: String,
        computed: 'getStatusString_(status_)',
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
    /** @private {ProvisioningObserverReceiver} */
    this.provisioningObserverReceiver_ = new ProvisioningObserverReceiver(
        /**
         * @type {!ProvisioningObserverInterface}
         */
        (this));

    this.shimlessRmaService_.observeProvisioningProgress(
        this.provisioningObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /**
   * @protected
   * @return {string}
   */
  getStatusString_() {
    if (!this.status_) {
      return '';
    }

    return this.i18n(provisioningStatusTextKeys[this.status_]);
  }

  /**
   * Implements ProvisioningObserver.onProvisioningUpdated()
   * TODO(joonbug): Add error handling and display failure using cr-dialog.
   * @protected
   * @param {!ProvisioningStatus} status
   * @param {number} progress
   */
  onProvisioningUpdated(status, progress) {
    this.status_ = status;

    // Transition to next state when provisioning is complete.
    if (this.status_ === ProvisioningStatus.kComplete) {
      this.shouldShowSpinner_ = false;
      executeThenTransitionState(
          this, () => this.shimlessRmaService_.provisioningComplete());
      return;
    }

    this.shouldShowSpinner_ = this.status_ === ProvisioningStatus.kInProgress;
    this.shouldShowRetryButton_ =
        this.status_ === ProvisioningStatus.kFailedBlocking ||
        this.status_ === ProvisioningStatus.kFailedNonBlocking;
  }

  /** @private */
  onRetryProvsioningButtonClicked_() {
    if (this.status_ !== ProvisioningStatus.kFailedBlocking &&
        this.status_ !== ProvisioningStatus.kFailedNonBlocking) {
      console.error('Provisioning has not failed.');
      return;
    }

    executeThenTransitionState(
        this, () => this.shimlessRmaService_.retryProvisioning());
  }
}

customElements.define(ReimagingProvisioningPage.is, ReimagingProvisioningPage);
