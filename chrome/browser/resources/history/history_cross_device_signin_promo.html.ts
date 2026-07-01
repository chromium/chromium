// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistoryCrossDeviceSigninPromoElement} from './history_cross_device_signin_promo.js';

export function getHtml(this: HistoryCrossDeviceSigninPromoElement) {
  return html`<!--_html_template_start_-->
<div id="promo" role="dialog">
  <cr-icon-button id="close" iron-icon="cr:close"
      aria-label="${this.i18n('historyEmbeddingsPromoClose')}"
      @click="${this.onCloseClick_}">
  </cr-icon-button>

  <img id="sync-history-illustration" class="sync-history-illustration" alt="">

  <div class="promo-content">
    <h2 class="title">
      ${this.i18n('signinOnPhonePromoTitle')}
    </h2>

    <div class="description">
      ${this.i18n('signinOnPhonePromoSubtitle')}
    </div>

    <div class="flex-row">
      <cr-button id="actionButton" class="action-button"
          @click="${this.onActionButtonClick_}">
        ${this.i18n('signinOnPhonePromoButton')}
      </cr-button>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
}
