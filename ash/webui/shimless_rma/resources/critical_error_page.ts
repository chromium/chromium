// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './shimless_rma_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {getTemplate} from './critical_error_page.html.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface} from './shimless_rma.mojom-webui.js';
import {disableAllButtons, focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'critical-error-page' is displayed when an unexpected error blocks RMA from
 * continuing.
 */

const CriticalErrorPageBase = I18nMixin(PolymerElement);

export class CriticalErrorPage extends CriticalErrorPageBase {
  static get is() {
    return 'critical-error-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // Set by shimless_rma.ts.
      allButtonsDisabled: Boolean,
    };
  }

  shimlessRmaService: ShimlessRmaServiceInterface = getShimlessRmaService();
  allButtonsDisabled: boolean;

  override ready() {
    super.ready();

    focusPageTitle(this);
  }

  protected onExitToLoginButtonClicked() {
    this.shimlessRmaService.criticalErrorExitToLogin();
    disableAllButtons(this, /* showBusyStateOverlay= */ true);
  }

  protected onRebootButtonClicked() {
    this.shimlessRmaService.criticalErrorReboot();
    disableAllButtons(this, /* showBusyStateOverlay= */ true);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CriticalErrorPage.is]: CriticalErrorPage;
  }
}

customElements.define(
    CriticalErrorPage.is, CriticalErrorPage);
