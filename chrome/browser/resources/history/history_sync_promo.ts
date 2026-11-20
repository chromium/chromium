// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {AccessPoint, HistorySignInState, SyncState} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import type {AccountInfo, HistoryIdentityState, PageCallbackRouter} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './history_sync_promo.css.js';
import {getHtml} from './history_sync_promo.html.js';

export class HistorySyncPromoElement extends CrLitElement {
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
  private callbackRouter_: PageCallbackRouter;
  private onAccountInfoDataReceivedListenerId_: number|null = null;
  private onIdentityStateChangedListenerId_: number|null = null;
  private signinPausedImpressionRecorded_: boolean = false;
  private accessor historyIdentityState_: HistoryIdentityState = {
    signIn: HistorySignInState.kSignedOut,
    tabsSync: SyncState.kTurnedOff,
    historySync: SyncState.kTurnedOff,
  };

  constructor() {
    super();
    this.callbackRouter_ = BrowserProxyImpl.getInstance().callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();

    BrowserProxyImpl.getInstance().handler.getInitialIdentityState().then(
        ({identityState}) => {
          this.updateHistoryIdentityState_(identityState);
        });

    this.onIdentityStateChangedListenerId_ =
        this.callbackRouter_.onIdentityStateChanged.addListener(
            (identityState: HistoryIdentityState) => {
              this.updateHistoryIdentityState_(identityState);
            });

    this.onAccountInfoDataReceivedListenerId_ =
        this.callbackRouter_.sendAccountInfo.addListener(
            this.handleAccountInfoChanged_.bind(this));
    BrowserProxyImpl.getInstance().handler.requestAccountInfo().then(
        ({accountInfo}) => this.handleAccountInfoChanged_(accountInfo));

    BrowserProxyImpl.getInstance()
        .handler.incrementHistoryPageHistorySyncPromoShownCount();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.onIdentityStateChangedListenerId_ !== null) {
      this.callbackRouter_.removeListener(
          this.onIdentityStateChangedListenerId_);
      this.onIdentityStateChangedListenerId_ = null;
    }
    if (this.onAccountInfoDataReceivedListenerId_ !== null) {
      this.callbackRouter_.removeListener(
          this.onAccountInfoDataReceivedListenerId_);
      this.onAccountInfoDataReceivedListenerId_ = null;
    }
  }

  private handleAccountInfoChanged_(accountInfo: AccountInfo) {
    this.accountInfo_ = accountInfo;
  }

  private updateHistoryIdentityState_(identityState: HistoryIdentityState) {
    this.historyIdentityState_ = identityState;
    this.maybeRecordSigninPendingOffered_();
  }

  private maybeRecordSigninPendingOffered_() {
    // Reset the flag if the state changes away from SIGN_IN_PENDING.
    if (!this.isSignInState_(HistorySignInState.kSignInPending)) {
      this.signinPausedImpressionRecorded_ = false;
      return;
    }

    // Don't record if the promo is not shown or was already recorded.
    if (!this.shown_ || this.signinPausedImpressionRecorded_) {
      return;
    }

    BrowserProxyImpl.getInstance().handler.recordSigninPendingOffered(
        AccessPoint.kHistoryPage);
    this.signinPausedImpressionRecorded_ = true;
  }

  protected onCloseClick_() {
    this.shown_ = false;
    BrowserProxyImpl.getInstance()
        .handler.recordHistoryPageHistorySyncPromoDismissed();
  }

  protected isSignInState_(state: HistorySignInState): boolean {
    return this.historyIdentityState_.signIn === state;
  }

  protected isHistorySyncTurnedOn_(): boolean {
    return this.historyIdentityState_.historySync === SyncState.kTurnedOn;
  }

  protected onTurnOnHistorySyncClick_() {
    BrowserProxyImpl.getInstance().handler.turnOnHistorySync(
        AccessPoint.kHistoryPage);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-sync-promo': HistorySyncPromoElement;
  }
}

customElements.define(HistorySyncPromoElement.is, HistorySyncPromoElement);
