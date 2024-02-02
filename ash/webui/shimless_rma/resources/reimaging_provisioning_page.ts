// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shimless_rma_shared.css.js';
import './base_page.js';
import './icons.html.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {createCustomEvent, FATAL_HARDWARE_ERROR, FatalHardwareEvent} from './events.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './reimaging_provisioning_page.html.js';
import {ProvisioningError, ProvisioningObserverReceiver, ProvisioningStatus, RmadErrorCode, ShimlessRmaServiceInterface} from './shimless_rma.mojom-webui.js';
import {executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';

declare global {
  interface HTMLElementEventMap {
    [FATAL_HARDWARE_ERROR]: FatalHardwareEvent;
  }
}

/**
 * @fileoverview
 * 'reimaging-provisioning-page' provisions the device then auto-transitions to
 * the next page once complete.
 */

/**
 * The prefix for a `ProvisioningError` displayed on the Hardware Error page.
 */
export const PROVISIONING_ERROR_CODE_PREFIX = 1000;

const ReimagingProvisioningPageBase = I18nMixin(PolymerElement);

export class ReimagingProvisioningPage extends ReimagingProvisioningPageBase {
  static get is() {
    return 'reimaging-provisioning-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.ts.
       */
      allButtonsDisabled: Boolean,

      status: {
        type: Object,
      },

      shouldShowSpinner: {
        type: Boolean,
        value: true,
      },
    };
  }

  allButtonsDisabled: boolean;
  protected status: ProvisioningStatus;
  protected shouldShowSpinner: boolean;
  private shimlessRmaService: ShimlessRmaServiceInterface =
      getShimlessRmaService();
  provisioningObserverReceiver: ProvisioningObserverReceiver =
      new ProvisioningObserverReceiver(this);

  constructor() {
    super();
    this.shimlessRmaService.observeProvisioningProgress(
        this.provisioningObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  override ready() {
    super.ready();

    focusPageTitle(this);
  }

  /**
   * Implements ProvisioningObserver.onProvisioningUpdated()
   */
  onProvisioningUpdated(
      status: ProvisioningStatus, _progress: number,
      error: ProvisioningError): void {
    const isErrorStatus = status === ProvisioningStatus.kFailedBlocking ||
        status === ProvisioningStatus.kFailedNonBlocking;
    const isWpError = isErrorStatus && error === ProvisioningError.kWpEnabled;

    if (isErrorStatus && !isWpError) {
      this.dispatchEvent(createCustomEvent(FATAL_HARDWARE_ERROR, {
        rmadErrorCode: RmadErrorCode.kProvisioningFailed,
        fatalErrorCode: (PROVISIONING_ERROR_CODE_PREFIX + error),
      }));
    }

    this.status = status;

    // Transition to next state when provisioning is complete.
    if (this.status === ProvisioningStatus.kComplete) {
      this.shouldShowSpinner = false;
      executeThenTransitionState(
          this, () => this.shimlessRmaService.provisioningComplete());
      return;
    }

    this.shouldShowSpinner =
        isWpError || this.status === ProvisioningStatus.kInProgress;

    if (isWpError) {
      const dialog: CrDialogElement|null =
          this.shadowRoot!.querySelector('#wpEnabledDialog');
      assert(dialog);
      dialog.showModal();
    }
  }

  protected onTryAgainButtonClick(): void {
    const dialog: CrDialogElement|null =
        this.shadowRoot!.querySelector('#wpEnabledDialog');
    assert(dialog);
    dialog.close();

    executeThenTransitionState(
        this, () => this.shimlessRmaService.retryProvisioning());
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ReimagingProvisioningPage.is]: ReimagingProvisioningPage;
  }
}

customElements.define(ReimagingProvisioningPage.is, ReimagingProvisioningPage);
