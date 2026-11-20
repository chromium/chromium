// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import {HistorySignInState} from 'chrome://resources/cr_components/history/history.mojom-webui.js';

import type {HistorySyncPromoElement} from './history_sync_promo.js';

export function getHtml(this: HistorySyncPromoElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.shown_ ? html`
  <div id="promo" role="dialog">
    <cr-icon-button id="close" iron-icon="cr:close"
        aria-label="$i18n{historyPromoClose}" @click="${this.onCloseClick_}">
    </cr-icon-button>

    ${!this.isSignInState_(HistorySignInState.kWebOnlySignedIn) ? html`
      <img id="sync-history-illustration" class="sync-history-illustration"
          alt="">
    ` : ''}

    <div class="promo-content">
      <h2 class="title">
        $i18n{historySyncPromoTitle}
      </h2>

      ${this.isSignInState_(HistorySignInState.kSignedOut) ? html`
        <div id="signed-out-description" class="description">
          $i18n{historySyncPromoBodySignedOut}
        </div>
      ` : ''}

      ${this.isSignInState_(HistorySignInState.kWebOnlySignedIn) ? html`
        <div id="web-only-signed-in-description"
            class="web-only-signed-in-description">
          $i18n{historySyncPromoBodyWebOnlySignedIn}
        </div>
      ` : ''}

      ${this.isSignInState_(HistorySignInState.kSignInPending) &&
              !this.isHistorySyncTurnedOn_() ?
          html`
        <div id="sign-in-pending-not-syncing-history-description"
            class="description">
          $i18n{historySyncPromoBodySignInPending}
        </div>
      ` : ''}

      ${this.isSignInState_(HistorySignInState.kSignInPending) &&
              this.isHistorySyncTurnedOn_() ?
          html`
        <div id="sign-in-pending-syncing-history-description"
            class="description">
          $i18n{historySyncPromoBodySignInPendingSyncHistoryOn}
        </div>
      ` : ''}

      ${this.isSignInState_(HistorySignInState.kSignedIn) ? html`
        <div id="signed-in-description" class="description">
          $i18n{historySyncPromoBodySignedIn}
        </div>
      ` : ''}

      <div class="flex-row">
        ${this.accountInfo_ &&
                this.isSignInState_(HistorySignInState.kWebOnlySignedIn) ?
            html`
          <div id="profile-info-row" class="profile-row">
            <img class="avatar" src="${this.accountInfo_.accountImageSrc}"
                alt="">
            <div>
              <div class="account-name">${this.accountInfo_.name}</div>
              <div class="account-email">${this.accountInfo_.email}</div>
            </div>
          </div>
        ` : ''}

        <!-- Button -->
        ${this.isSignInState_(HistorySignInState.kSignInPending) &&
                this.isHistorySyncTurnedOn_() ?
            html`
          <cr-button id="verify-its-you-button" class="action-button"
              @click="${this.onTurnOnHistorySyncClick_}">
            $i18n{verifyItsYou}
          </cr-button>
        ` : html`
          <cr-button id="sync-history-button" class="action-button"
              @click="${this.onTurnOnHistorySyncClick_}">
            $i18n{turnOnSyncHistoryButton}
          </cr-button>
        `}
      </div>

    </div>
  </div>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
