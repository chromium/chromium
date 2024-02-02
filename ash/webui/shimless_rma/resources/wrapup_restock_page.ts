// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './shimless_rma_shared.css.js';
import './base_page.js';
import './icons.html.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface} from './shimless_rma.mojom-webui.js';
import {executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';
import {getTemplate} from './wrapup_restock_page.html.js';

/**
 * @fileoverview
 * 'wrapup-restock-page' is the page that informs the repair technician they
 * can shut down the device and restock the mainboard or continue to finalize
 * the repair if the board is being used to repair another device.
 */

const WrapupRestockPageBase = I18nMixin(PolymerElement);

export class WrapupRestockPage extends WrapupRestockPageBase {
  static get is() {
    return 'wrapup-restock-page' as const;
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
    };
  }

  allButtonsDisabled: boolean;
  private shimlessRmaService: ShimlessRmaServiceInterface =
      getShimlessRmaService();

  constructor() {
    super();
    this.shimlessRmaService = getShimlessRmaService();
  }

  override ready() {
    super.ready();

    focusPageTitle(this);
  }

  protected onShutdownButtonClicked(): void {
    executeThenTransitionState(
        this, () => this.shimlessRmaService.shutdownForRestock());
  }

  protected onRestockContinueButtonClicked(): void {
    executeThenTransitionState(
        this, () => this.shimlessRmaService.continueFinalizationAfterRestock());
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [WrapupRestockPage.is]: WrapupRestockPage;
  }
}

customElements.define(WrapupRestockPage.is, WrapupRestockPage);
