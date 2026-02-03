// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement, html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SecurityIcon} from './browser.mojom-webui.js';
import {GuestHandlerRemote} from './browser.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {getCss} from './webview.css.js';

const SURFACE_EMBED_MIME_TYPE = 'application/x-chromium-surface-embed';

export interface WebviewElement {
  $: {
    iframe: HTMLIFrameElement,
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
    if (this.enableSurfaceEmbed) {
      return html`<embed id="embed" type="${SURFACE_EMBED_MIME_TYPE}"
          data-content-id="${this.guestId}"></embed>`;
    } else {
      return html`<iframe id="iframe"></iframe>`;
    }
  }

  static override get properties() {
    return {
      guestId: {type: Number},
    };
  }

  accessor guestId: number = -1;
  private attached: boolean = false;

  // Whether to use surface embed instead of guest contents.
  protected enableSurfaceEmbed: boolean =
      loadTimeData.getBoolean('enableSurfaceEmbed');

  override async connectedCallback() {
    super.connectedCallback();
    await this.tryToAttach();
  }

  protected async tryToAttach() {
    // For surface embed, the plugin reads data-content-id directly from the
    // element attributes.
    if (this.enableSurfaceEmbed) {
      return;
    }

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

  protected getContentElement(): HTMLElement {
    // Returns whichever content element is currently active based on the mode.
    return this.enableSurfaceEmbed ? this.$.embed : this.$.iframe;
  }

  private attachGuestToIframe(guestId: number, iframe: HTMLIFrameElement) {
    assert(!this.enableSurfaceEmbed);
    const iframeContentWindow = iframe.contentWindow;
    assert(iframeContentWindow);
    chrome.browser.attachIframeGuest(guestId, iframeContentWindow);
  }

  private async whenIframeContentWindowAvailable_(iframe: HTMLIFrameElement):
      Promise<void> {
    assert(!this.enableSurfaceEmbed);
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
}

export class TabWebviewElement extends WebviewElement {
  static override get is() {
    return 'cr-tab-webview';
  }

  tabId: string;
  private guestHandler: GuestHandlerRemote = new GuestHandlerRemote();

  constructor(tabId: string) {
    super();
    this.tabId = tabId;
    this.attachTabContents();
  }

  setActive(active: boolean) {
    if (active) {
      this.classList.add('active');
      this.getContentElement().focus();
    } else {
      this.classList.remove('active');
    }
  }

  openPageInfoMenu() {
    this.guestHandler.openPageInfoMenu();
  }

  async getSecurityIcon(): Promise<SecurityIcon> {
    const {securityIcon} = await this.guestHandler.getSecurityIcon();
    return securityIcon;
  }

  private attachTabContents() {
    BrowserProxy.getPageHandler()
        .getGuestIdForTabId(
            this.tabId, this.guestHandler.$.bindNewPipeAndPassReceiver())
        .then(({guestId}) => {
          this.guestId = guestId;
          this.tryToAttach();
        });
  }

  goBack() {
    this.guestHandler.goBack();
  }

  goForward() {
    this.guestHandler.goForward();
  }

  reload() {
    this.guestHandler.reload();
  }

  stopLoading() {
    this.guestHandler.stopLoading();
  }

  async canGoBack(): Promise<boolean> {
    const {canGoBack} = await this.guestHandler.canGoBack();
    return canGoBack;
  }

  async canGoForward(): Promise<boolean> {
    const {canGoForward} = await this.guestHandler.canGoForward();
    return canGoForward;
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
