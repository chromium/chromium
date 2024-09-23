// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './shimless_rma_shared.css.js';
import './base_page.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {createCustomEvent, FatalHardwareEvent, FATAL_HARDWARE_ERROR} from './events.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {FinalizationError, FinalizationObserverReceiver, FinalizationStatus, RmadErrorCode, ShimlessRmaServiceInterface} from './shimless_rma.mojom-webui.js';
import {executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';
import {getTemplate} from './wrapup_finalize_page.html.js';

declare global {
  interface HTMLElementEventMap {
    [FATAL_HARDWARE_ERROR]: FatalHardwareEvent;
  }
}

const finalizationStatusTextKeys: {[key in FinalizationStatus]: string} = {
  [FinalizationStatus.kInProgress]: 'finalizePageProgressText',
  [FinalizationStatus.kComplete]: 'finalizePageCompleteText',
  [FinalizationStatus.kFailedBlocking]: '',
  [FinalizationStatus.kFailedNonBlocking]: '',
};

/**
 * @fileoverview
 * 'wrapup-finalize-page' wait for device finalization and hardware verification
 * to be completed.
 */

/**
 * The prefix for a `FinalizationError` displayed on the Hardware Error page.
 */
export const FINALIZATION_ERROR_CODE_PREFIX = 2000;

const WrapupFinalizePageBase = I18nMixin(PolymerElement);

export class WrapupFinalizePage extends WrapupFinalizePageBase {
  static get is() {
    return 'wrapup-finalize-page' as const;
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

      finalizationMessage: {
        type: String,
        value: '',
      },
    };
  }

  allButtonsDisabled: boolean;
  // Receiver responsible for observing finalization progress and state.
  finalizationObserverReceiver: FinalizationObserverReceiver = new FinalizationObserverReceiver(this);
  protected finalizationMessage: string;
  private shimlessRmaService: ShimlessRmaServiceInterface =
      getShimlessRmaService();

  constructor() {
    super();
    this.shimlessRmaService.observeFinalizationStatus(
        this.finalizationObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  override ready() {
    super.ready();

    focusPageTitle(this);
  }

  onFinalizationUpdated(
      status: FinalizationStatus, _progress: number,
      error: FinalizationError): void {
    if (status === FinalizationStatus.kFailedBlocking ||
        status === FinalizationStatus.kFailedNonBlocking) {
      this.dispatchEvent(createCustomEvent(FATAL_HARDWARE_ERROR, {
        rmadErrorCode: RmadErrorCode.kFinalizationFailed,
        fatalErrorCode: (FINALIZATION_ERROR_CODE_PREFIX + error),
      }));
    } else {
      this.finalizationMessage = this.i18n(finalizationStatusTextKeys[status]);

      if (status === FinalizationStatus.kComplete) {
        executeThenTransitionState(
            this, () => this.shimlessRmaService.finalizationComplete());
        return;
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [WrapupFinalizePage.is]: WrapupFinalizePage;
  }
}

customElements.define(WrapupFinalizePage.is, WrapupFinalizePage);
