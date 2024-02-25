// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './shimless_rma_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {getTemplate} from './hardware_error_page.html.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface} from './shimless_rma.mojom-webui.js';
import {disableAllButtons, focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'hardware-error-page' is displayed when an unexpected error blocks RMA from
 * continuing.
 */

const HardwareErrorPageBase = I18nMixin(PolymerElement);

export class HardwareErrorPage extends HardwareErrorPageBase {
  static get is() {
    return 'hardware-error-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.js.
       */
      allButtonsDisabled: Boolean,

      /**
       * Set by shimless_rma.js.
       */
      errorCode: Number,
    };
  }

  shimlessRmaService: ShimlessRmaServiceInterface = getShimlessRmaService();
  errorCode: number;
  allButtonsDisabled: boolean;

  override ready() {
    super.ready();

    focusPageTitle(this);
  }

  protected onShutDownButtonClicked(): void {
    this.shimlessRmaService.shutDownAfterHardwareError();
    disableAllButtons(this, /* showBusyStateOverlay= */ true);
  }

  protected getErrorCodeString(): string {
    return this.i18n('hardwareErrorCode', this.errorCode);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [HardwareErrorPage.is]: HardwareErrorPage;
  }
}

customElements.define(HardwareErrorPage.is, HardwareErrorPage);
