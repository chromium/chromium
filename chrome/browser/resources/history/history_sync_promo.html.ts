// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistorySyncPromoElement} from './history_sync_promo.js';

export function getHtml(this: HistorySyncPromoElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.shown_ ? html`
<div id="promo" role="dialog">
  <cr-icon-button id="close" iron-icon="cr:close"
      aria-label="$i18n{historyEmbeddingsPromoClose}"
      @click="${this.onCloseClick_}">
  </cr-icon-button>

  <img class="sync-history-illustration" alt="">

  <div class="text">
    <h2 class="title">
      $i18n{historySyncPromoTitle}
    </h2>
    <div id="signed-out-description" class="description">
        $i18n{historySyncPromoBodySignedOut}
    </div>
    <cr-button id="sync-history-button" class="action-button">
      $i18n{turnOnSyncHistoryButton}
    </cr-button>
  </div>
</div>` : ''}
<!--_html_template_end_-->`;
// clang-format on
}
