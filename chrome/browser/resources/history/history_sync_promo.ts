// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';

import {assert} from 'chrome://resources/js/assert.js';
import {BrowserServiceImpl} from './browser_service.js';
import {HistorySignInState, SyncState} from './constants.js';
import type {HistoryIdentityState} from './externs.js';
import type {AccountInfo} from 'chrome://resources/cr_components/history/history.mojom-webui.js';

import {getCss} from './history_sync_promo.css.js';
import {getHtml} from './history_sync_promo.html.js';

const HistorySyncPromoElementBase = WebUiListenerMixinLit(CrLitElement);

export class HistorySyncPromoElement extends HistorySyncPromoElementBase {
  static get is() {
    return 'history-sync-promo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      shown_: {type: Boolean},
      historyIdentityState_: {type: Object},
      accountInfo_: {type: Object},
    };
  }

  protected accessor shown_: boolean = true;
  protected accessor accountInfo_: AccountInfo|null = null;
  private onAccountInfoDataReceivedListenerId_: number|null = null;
  private accessor historyIdentityState_: HistoryIdentityState = {
    signIn: HistorySignInState.SIGNED_OUT,
    tabsSync: SyncState.TURNED_OFF,
    historySync: SyncState.TURNED_OFF,
  };

  override connectedCallback() {
    super.connectedCallback();

    BrowserServiceImpl.getInstance().getInitialIdentityState().then(
        (identityState: HistoryIdentityState) => {
          this.historyIdentityState_ = identityState;
        });

    this.addWebUiListener(
        'history-identity-state-changed',
        (identityState: HistoryIdentityState) => this.historyIdentityState_ =
            identityState);

    this.onAccountInfoDataReceivedListenerId_ =
        BrowserServiceImpl.getInstance()
            .callbackRouter.sendAccountInfo.addListener(
                this.handleAccountInfoChanged_.bind(this));
    BrowserServiceImpl.getInstance().handler.requestAccountInfo().then(
        ({accountInfo}) => this.handleAccountInfoChanged_(accountInfo));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.onAccountInfoDataReceivedListenerId_);
    BrowserServiceImpl.getInstance().callbackRouter.removeListener(
        this.onAccountInfoDataReceivedListenerId_);
    this.onAccountInfoDataReceivedListenerId_ = null;
  }

  private handleAccountInfoChanged_(accountInfo: AccountInfo) {
    this.accountInfo_ = accountInfo;
  }

  protected onCloseClick_() {
    this.shown_ = false;
    BrowserServiceImpl.getInstance()
        .handler.incrementHistoryPageHistorySyncPromoShownCount();
  }

  protected isSignInState_(state: HistorySignInState): boolean {
    return this.historyIdentityState_.signIn === state;
  }

  protected isHistorySyncTurnedOn_(): boolean {
    return this.historyIdentityState_.historySync === SyncState.TURNED_ON;
  }

  protected onTurnOnHistorySyncClick_() {
    BrowserServiceImpl.getInstance().handler.turnOnHistorySync();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-sync-promo': HistorySyncPromoElement;
  }
}

customElements.define(HistorySyncPromoElement.is, HistorySyncPromoElement);
