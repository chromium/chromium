// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import {HistorySignInState} from './constants.js';

import type {HistorySyncedDeviceManagerElement} from './synced_device_manager.js';

export function getHtml(this: HistorySyncedDeviceManagerElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="synced-device-list" class="history-cards"
    ?hidden="${!this.syncedDevices_.length}">
  ${this.syncedDevices_.map((syncedDevice, index) => html`
    <history-synced-device-card
        .device="${syncedDevice.device}"
        .lastUpdateTime="${syncedDevice.lastUpdateTime}"
        .tabs="${syncedDevice.tabs}"
        .separatorIndexes="${syncedDevice.separatorIndexes}"
        .searchTerm="${this.searchTerm}"
        .sessionTag="${syncedDevice.tag}"
        ?opened="${syncedDevice.opened}"
        @opened-changed="${this.onCardOpenedChanged_}"
        data-index="${index}">
    </history-synced-device-card>
  `)}
</div>
<div id="no-synced-tabs" class="centered-message"
    ?hidden="${!this.showNoSyncedMessage_()}">
  ${this.noSyncedTabsMessage_()}
</div>

<div id="sign-in-guide"
    ?hidden="${!this.showSignInGuide_()
      || this.replaceSyncPromosWithSignInPromos_}">
  <div id="sync-promo-illustration"></div>
  <div id="turn-on-sync-promo">$i18n{turnOnSyncPromo}</div>
  <div id="turn-on-sync-promo-desc">$i18n{turnOnSyncPromoDesc}</div>
  <cr-button id="turn-on-sync-button" class="action-button"
      @click="${this.onTurnOnSyncClick_}">
    $i18n{turnOnSyncButton}
  </cr-button>
</div>

<if expr="not is_chromeos">
  ${this.shouldShowHistorySyncOptIn_() ? html`
    <div id="history-sync-optin" class="history-sync-optin">
      ${this.isSignInState_(HistorySignInState.SIGNED_OUT)
        || this.isSignInState_(HistorySignInState.WEB_ONLY_SIGNED_IN) ? html`
        <div class="image-container">
          <img class="sync-history-illustration" alt="">
        </div>
        <h1 class="sync-history-promo">$i18n{turnOnSyncHistoryPromo}</h1>
        <div id="signed-out-sync-history-promo-desc"
            class="sync-history-promo-desc">
          $i18n{syncHistoryPromoBodySignedOut}
        </div>
      ` : ''}

      ${this.isSignInState_(HistorySignInState.WEB_ONLY_SIGNED_IN)
        && this.accountInfo_ ? html`
        <div class="profile-row">
          <img id="profile-icon" class="profile-icon"
              src="${this.accountInfo_.accountImageSrc.url}">
          <div class="account-info-container">
            <div id="account-name" class="account-name">
              ${this.accountInfo_.name}</div>
            <div id="account-email" class="account-email">
              ${this.accountInfo_.email}</div>
          </div>
        </div>
      ` : ''}

      ${this.isSignInStatePending_() && this.accountInfo_ ? html`
        <div class="image-container">
          <img class="sync-history-promo-avatar-illustration" alt="">
          <img id="sign-in-pending-avatar" class="avatar"
              src="${this.accountInfo_.accountImageSrc.url}" alt="">
        </div>
        <h1 class="sync-history-promo">$i18n{turnOnSyncHistoryPromo}</h1>

        ${this.isSignInState_(
          HistorySignInState.SIGN_IN_PENDING_NOT_SYNCING_TABS) ? html`
          <div id="sign-in-pending-sync-history-promo-desc"
              class="sync-history-promo-desc">
            $i18n{syncHistoryPromoBodyPendingSignIn}
          </div>
        ` : ''}

        ${this.isSignInState_(
          HistorySignInState.SIGN_IN_PENDING_SYNCING_TABS) ? html`
          <div id="sign-in-pending-sync-history-promo-desc-sync-history-on"
            class="sync-history-promo-desc">
          $i18n{syncHistoryPromoBodyPendingSignInSyncHistoryOn}
          </div>
        ` : ''}
      ` : ''}

      ${this.isSignInState_(HistorySignInState.SIGNED_IN_NOT_SYNCING_TABS)
        && this.accountInfo_ ? html`
          <div class="image-container">
            <img class="sync-history-promo-avatar-illustration" alt="">
            <img id="signed-in-avatar" class="avatar"
                src="${this.accountInfo_.accountImageSrc.url}" alt="">
          </div>
          <h1 class="sync-history-promo">$i18n{turnOnSyncHistoryPromo}</h1>
          <div id="signed-in-sync-history-promo-desc"
              class="sync-history-promo-desc">
            $i18n{turnOnSignedInSyncHistoryPromoBodySignInSyncOff}
          </div>
        ` : ''}

      <!-- Button -->
      ${this.isSignInState_(HistorySignInState.SIGN_IN_PENDING_SYNCING_TABS)
        ? html`
          <cr-button id="verify-its-you-button" class="action-button"
              @click="${this.onTurnOnHistorySyncClick_}">$i18n{verifyItsYou}
          </cr-button>` : html`
          <cr-button id="sync-history-button" class="action-button"
              @click="${this.onTurnOnHistorySyncClick_}">
            $i18n{turnOnSyncHistoryButton}
          </cr-button>`}
    </div>
  ` : ''}
</if>

<cr-lazy-render-lit id="menu" .template='${() => html`
  <cr-action-menu role-description="$i18n{menu}">
    <button id="menuOpenButton" class="dropdown-item"
        @click="${this.onOpenAllClick_}">
      $i18n{openAll}
    </button>
    <button id="menuDeleteButton" class="dropdown-item"
        @click="${this.onDeleteSessionClick_}">
      $i18n{deleteSession}
    </button>
  </cr-action-menu>
`}'>
</cr-lazy-render-lit>
<!--_html_template_end_-->`;
  // clang-format on
}
