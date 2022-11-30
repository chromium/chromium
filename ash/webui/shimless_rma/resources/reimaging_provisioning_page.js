// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shimless_rma_shared_css.js';
import './base_page.js';
import './icons.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ProvisioningError, ProvisioningObserverInterface, ProvisioningObserverReceiver, ProvisioningStatus, RmadErrorCode, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {disableNextButton, enableNextButton, executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'reimaging-provisioning-page' provisions the device then auto-transitions to
 * the next page once complete.
 */

/**
 * The prefix for a `ProvisioningError` displayed on the Hardware Error page.
 * @type {number}
 */
export const PROVISIONING_ERROR_CODE_PREFIX = 1000;

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

      /** @protected {boolean} */
      shouldShowSpinner_: {
        type: Boolean,
        value: true,
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

  /** @override */
  ready() {
    super.ready();

    focusPageTitle(this);
  }

  /**
   * Implements ProvisioningObserver.onProvisioningUpdated()
   * @param {!ProvisioningStatus} status
   * @param {number} progress
   * @param {!ProvisioningError} error
   * @protected
   */
  onProvisioningUpdated(status, progress, error) {
    const isErrorStatus = status === ProvisioningStatus.kFailedBlocking ||
        status === ProvisioningStatus.kFailedNonBlocking;
    const isWpError = isErrorStatus && error === ProvisioningError.kWpEnabled;

    if (isErrorStatus && !isWpError) {
      this.dispatchEvent(new CustomEvent('fatal-hardware-error', {
        bubbles: true,
        composed: true,
        detail: {
          rmadErrorCode: RmadErrorCode.kProvisioningFailed,
          fatalErrorCode: (PROVISIONING_ERROR_CODE_PREFIX + error),
        },
      }));
    }

    this.status_ = status;

    // Transition to next state when provisioning is complete.
    if (this.status_ === ProvisioningStatus.kComplete) {
      this.shouldShowSpinner_ = false;
      executeThenTransitionState(
          this, () => this.shimlessRmaService_.provisioningComplete());
      return;
    }

    this.shouldShowSpinner_ =
        isWpError || this.status_ === ProvisioningStatus.kInProgress;

    if (isWpError) {
      const dialog = /** @type {!CrDialogElement} */ (
          this.shadowRoot.querySelector('#wpEnabledDialog'));
      dialog.showModal();
    }
  }

  /** @protected */
  onTryAgainButtonClick_() {
    const dialog = /** @type {!CrDialogElement} */ (
        this.shadowRoot.querySelector('#wpEnabledDialog'));
    dialog.close();

    executeThenTransitionState(
        this, () => this.shimlessRmaService_.retryProvisioning());
  }
}

customElements.define(ReimagingProvisioningPage.is, ReimagingProvisioningPage);
