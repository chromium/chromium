// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

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

  override async connectedCallback() {
    super.connectedCallback();
    assert(this.guestId !== undefined);

    // Wait until iframe.contentWindow becomes available.
    await this.whenIframeContentWindowAvailable_(this.$.iframe);
    if (this.isConnected) {
      this.attachGuestToIframe_(this.guestId, this.$.iframe);
    }
  }

  private attachGuestToIframe_(guestId: number, iframe: HTMLIFrameElement) {
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

declare global {
  interface HTMLElementTagNameMap {
    'cr-webview': WebviewElement;
  }
}

customElements.define(WebviewElement.is, WebviewElement);
