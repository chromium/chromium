// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {CrLitElement, html} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxy} from './browser_proxy.js';
import {getCss} from './webview.css.js';

export interface WebviewElement {
  $: {
    iframe: HTMLIFrameElement,
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
    return html`<iframe id="iframe"></iframe>`;
  }

  static override get properties() {
    return {
      guestId: {type: Number},
    };
  }

  protected accessor guestId: number = -1;
  private attached: boolean = false;

  override async connectedCallback() {
    super.connectedCallback();
    await this.tryToAttach();
  }

  protected async tryToAttach() {
    if (this.attached || this.guestId === -1) {
      return;
    }
    this.attached = true;

    // Wait until iframe.contentWindow becomes available.
    if (!this.$.iframe.contentWindow) {
      await this.whenIframeContentWindowAvailable_(this.$.iframe);
    }
    this.attachGuestToIframe(this.guestId, this.$.iframe);
  }

  private attachGuestToIframe(guestId: number, iframe: HTMLIFrameElement) {
    const iframeContentWindow = iframe.contentWindow;
    assert(iframeContentWindow);
    chrome.browser.attachIframeGuest(guestId, iframeContentWindow);
  }

  private async whenIframeContentWindowAvailable_(iframe: HTMLIFrameElement):
      Promise<void> {
    return new Promise(resolve => {
      // TODO(webium): find a way to get notified when the contentWindow is
      // ready. This is a workaround to poll every 100ms.
      const intervalId = setInterval(() => {
        if (!iframe.contentWindow) {
          return;
        }
        clearInterval(intervalId);
        resolve();
      }, 100);
    });
  }

  goBack() {
    BrowserProxy.getPageHandler().goBack(this.guestId);
  }

  goForward() {
    BrowserProxy.getPageHandler().goForward(this.guestId);
  }

  async canGoBack(): Promise<boolean> {
    const {canGoBack} =
        await BrowserProxy.getPageHandler().canGoBack(this.guestId);
    return canGoBack;
  }

  async canGoForward(): Promise<boolean> {
    const {canGoForward} =
        await BrowserProxy.getPageHandler().canGoForward(this.guestId);
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

  setActive(active: boolean) {
    if (active) {
      this.classList.add('active');
    } else {
      this.classList.remove('active');
    }
  }

  private attachTabContents() {
    BrowserProxy.getPageHandler()
        .getGuestIdForTabId(this.tabId)
        .then(({guestId}) => {
          this.guestId = guestId;
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
