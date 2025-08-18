// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './content_region.css.js';
import {TabWebviewElement} from './webview.js';

export class ContentRegion extends CrLitElement {
  static get is() {
    return 'content-region';
  }

  static override get styles() {
    return getCss();
  }

  // Please DO NOT trigger Lit's reactive update. The DOM tree of this component
  // must be manipulated manually.
  // A <cr-webview>'s render frame is connected to its parent frame by
  // RenderFrameProxyHost at the browser side. This proxy is disconnected as
  // soon as the <cr-webview> element is detached from its parent. Re-attaching
  // the element does not re-estabilish the proxy.
  override render() {
    return '';
  }
  override shouldUpdate(_: Map<string, any>) {
    return false;
  }

  private webviews: Map<string, TabWebviewElement> = new Map();
  activeWebview?: TabWebviewElement;

  activateTab(tabId: string) {
    const webview = this.webviews.get(tabId);
    assert(webview);
    if (this.activeWebview === webview) {
      return;
    }

    if (this.activeWebview) {
      this.activeWebview.setActive(false);
    }
    this.activeWebview = webview;
    webview.setActive(true);
  }

  hasTab(tabId: string) {
    return this.webviews.has(tabId);
  }

  createWebView(tabId: string, isActive: boolean) {
    if (this.hasTab(tabId)) {
      return;
    }

    const webview = new TabWebviewElement(tabId);
    this.webviews.set(tabId, webview);
    this.shadowRoot.appendChild(webview);
    if (isActive) {
      this.activateTab(tabId);
    }
    this.requestUpdate();
  }

  removeTab(tabId: string) {
    const webview = this.webviews.get(tabId);
    assert(webview);
    this.webviews.delete(tabId);
    webview.remove();
    this.requestUpdate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'content-region': ContentRegion;
  }
}

customElements.define(ContentRegion.is, ContentRegion);
