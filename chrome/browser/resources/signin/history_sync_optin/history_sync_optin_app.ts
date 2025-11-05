// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {HistorySyncOptInBrowserProxyImpl} from './browser_proxy.js';
import type {HistorySyncOptInBrowserProxy} from './browser_proxy.js';
import type {AccountInfo} from './history_sync_optin.mojom-webui.js';
import {LaunchContext, ScreenMode} from './history_sync_optin.mojom-webui.js';
import {getCss} from './history_sync_optin_app.css.js';
import {getHtml} from './history_sync_optin_app.html.js';

const HistorySyncOptinAppElementBase = I18nMixinLit(CrLitElement);

export class HistorySyncOptinAppElement extends HistorySyncOptinAppElementBase {
  static get is() {
    return 'history-sync-optin-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      accountImageSrc_: {type: String},
      buttonsDisabled_: {type: Boolean},
      isModal_: {
        type: Boolean,
        reflect: true,
      },
      screenMode_: {type: Number},
    };
  }

  protected accessor accountImageSrc_: string =
      loadTimeData.getString('accountPictureUrl');
  protected launchContext_: LaunchContext =
      loadTimeData.getInteger('launchContext') as LaunchContext;
  protected accessor isModal_: boolean = this.isLaunchContext_(LaunchContext.kModal);
  protected accessor buttonsDisabled_: boolean = false;

  private historySyncOptInBrowserProxy_: HistorySyncOptInBrowserProxy =
      HistorySyncOptInBrowserProxyImpl.getInstance();
  private onAccountInfoDataReceivedListenerId_: number|null = null;
  private onScreenModeDataReceivedListenerId_: number|null = null;
  private accessor screenMode_: ScreenMode = ScreenMode.kPending;

  override connectedCallback() {
    super.connectedCallback();

    this.onAccountInfoDataReceivedListenerId_ =
        this.historySyncOptInBrowserProxy_.callbackRouter.sendAccountInfo
            .addListener(this.handleAccountInfoChanged_.bind(this));

    this.onScreenModeDataReceivedListenerId_ =
        this.historySyncOptInBrowserProxy_.callbackRouter.sendScreenMode
            .addListener(this.handleScreenModeChanged_.bind(this));

    this.historySyncOptInBrowserProxy_.handler.requestAccountInfo();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.onAccountInfoDataReceivedListenerId_);
    this.historySyncOptInBrowserProxy_.callbackRouter.removeListener(
        this.onAccountInfoDataReceivedListenerId_);
    this.onAccountInfoDataReceivedListenerId_ = null;

    assert(this.onScreenModeDataReceivedListenerId_);
    this.historySyncOptInBrowserProxy_.callbackRouter.removeListener(
        this.onScreenModeDataReceivedListenerId_);
    this.onScreenModeDataReceivedListenerId_ = null;
  }

  protected onReject_() {
    this.buttonsDisabled_ = true;
    this.historySyncOptInBrowserProxy_.handler.reject();
  }

  protected onAccept_() {
    this.buttonsDisabled_ = true;
    this.historySyncOptInBrowserProxy_.handler.accept();
  }

  protected isLaunchContext_(launchContext: LaunchContext): boolean {
    return this.launchContext_ === launchContext;
  }

  protected getRejectButtonClass_(): string {
    switch (this.screenMode_) {
      case ScreenMode.kPending:
        return 'visibility-hidden';
      case ScreenMode.kUnrestricted:
        return 'tonal-button';
      default:
        // Default cr-button styling, appears equally weighted with the other
        // button.
        return '';
    }
  }

  protected getAcceptButtonClass_(): string {
    switch (this.screenMode_) {
      case ScreenMode.kPending:
        return 'visibility-hidden';
      case ScreenMode.kUnrestricted:
        return 'action-button';
      default:
        // Default cr-button styling, appears equally weighted with the other
        // button.
        return '';
    }
  }

  private handleAccountInfoChanged_(accountInfo: AccountInfo) {
    this.accountImageSrc_ = accountInfo.accountImageSrc.url;
  }

  private handleScreenModeChanged_(screenMode: ScreenMode) {
    this.screenMode_ = screenMode;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-sync-optin-app': HistorySyncOptinAppElement;
  }
}

customElements.define(
    HistorySyncOptinAppElement.is, HistorySyncOptinAppElement);
