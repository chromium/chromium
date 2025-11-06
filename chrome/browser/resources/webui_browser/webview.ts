// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {CrLitElement, html, render} from '//resources/lit/v3_0/lit.rollup.js';

import type {SecurityIcon} from './browser.mojom-webui.js';
import {GuestHandlerRemote} from './browser.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {getCss} from './webview.css.js';

export interface WebviewElement {
  $: {
    embed: HTMLEmbedElement,
  };
}

export class WebviewElement extends CrLitElement {
  static get is() {
    return 'cr-webview';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return '';
  }
  override shouldUpdate(_: Map<string, any>) {
    return false;
  }

  static override get properties() {
    return {
      guestId: {type: Number},
    };
  }

  accessor guestId: number = -1;
  private attached: boolean = false;
  private active: boolean = false;
  guestHandler?: GuestHandlerRemote;

  override async connectedCallback() {
    await super.connectedCallback();
    this.tryToAttach();
  }

  protected tryToAttach() {
    if (this.attached || this.guestId === -1) {
      return;
    }

    let embed = html`<embed id="embed"
          type="application/x-google-chrome-secure-embed"
          data-content-id="${this.guestId}"
          src="about:blank">
         </embed>`;
    render(embed, this.shadowRoot, this.renderOptions);
    this.attached = true;
    if (this.active) {
      this.$.embed.focus();
    }
  }

  setActive(active: boolean) {
    this.active = active;
    if (this.attached && active) {
      this.$.embed.focus();
    }
  }

  goBack() {
    assert(this.guestHandler);
    this.guestHandler.goBack();
  }

  goForward() {
    assert(this.guestHandler);
    this.guestHandler.goForward();
  }

  reload() {
    if (this.guestHandler) {
      this.guestHandler.reload();
    }
  }

  stopLoading() {
    if (this.guestHandler) {
      this.guestHandler.stopLoading();
    }
  }

  async canGoBack(): Promise<boolean> {
    if (!this.guestHandler) {
      return false;
    }
    const {canGoBack} = await this.guestHandler.canGoBack();
    return canGoBack;
  }

  async canGoForward(): Promise<boolean> {
    if (!this.guestHandler) {
      return false;
    }
    const {canGoForward} = await this.guestHandler.canGoForward();
    return canGoForward;
  }
}

export class TabWebviewElement extends WebviewElement {
  static override get is() {
    return 'cr-tab-webview';
  }

  tabId: string;

  constructor(tabId: string) {
    super();
    this.tabId = tabId;
    this.attachTabContents();
  }

  override setActive(active: boolean) {
    if (active) {
      this.classList.add('active');
    } else {
      this.classList.remove('active');
    }
    super.setActive(active);
  }

  openPageInfoMenu() {
    if (this.guestHandler) {
      this.guestHandler.openPageInfoMenu();
    }
  }

  async getSecurityIcon(): Promise<SecurityIcon> {
    assert(this.guestHandler);
    const {securityIcon} = await this.guestHandler.getSecurityIcon();
    return securityIcon;
  }

  private attachTabContents() {
    const handler = new GuestHandlerRemote();
    BrowserProxy.getPageHandler()
        .getGuestIdForTabId(this.tabId, handler.$.bindNewPipeAndPassReceiver())
        .then(({guestId}) => {
          this.guestId = guestId;
          this.guestHandler = handler;
          this.tryToAttach();
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-webview': WebviewElement;
    'cr-tab-webview': TabWebviewElement;
  }
}

customElements.define(WebviewElement.is, WebviewElement);
customElements.define(TabWebviewElement.is, TabWebviewElement);
