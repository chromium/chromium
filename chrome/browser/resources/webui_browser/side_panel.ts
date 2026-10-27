// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './bookmarks/bookmarks.js';
import './webview.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from './browser.mojom-webui.js';
import {getCss} from './side_panel.css.js';
import {getHtml} from './side_panel.html.js';
import type {WebviewElement} from './webview.js';

export class SidePanelElement extends CrLitElement {
  static get is() {
    return 'side-panel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showing_: {state: true, type: Boolean},
      title_: {state: true, type: String},
      webView: {state: true, type: Object},
      // TODO(crbug.com/501483829): we should be passing in the content type
      // from the outer element and not rely on this types of booleans to change
      // the content.
      showBookmarks_: {state: true, type: Boolean},
    };
  }

  accessor webView: WebviewElement|null = null;
  protected accessor title_: string = '';
  protected accessor showing_: boolean = false;
  protected accessor showBookmarks_: boolean = false;

  show(guestContentsId: string, title: string) {
    this.webView = document.createElement('cr-webview');
    this.webView.guestId = guestContentsId;
    this.showBookmarks_ = false;
    this.title_ = title;
    this.showing_ = true;
  }

  showBookmarks() {
    this.showBookmarks_ = true;
  }

  isShowingBookmarks(): boolean {
    return this.showBookmarks_;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    const changedPrivatedProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivatedProperties.has('showBookmarks_')) {
      if (this.showBookmarks_) {
        this.showing_ = true;
      }
    }
  }

  protected onCloseClick_(_e: Event) {
    this.close();
  }

  async close() {
    this.showing_ = false;
    this.webView = null;
    this.showBookmarks_ = false;
    await this.updateComplete;
    this.dispatchEvent(new Event('side-panel-closed', {bubbles: true}));
    browserProxyFactory.getInstance().handler.onSidePanelClosed();
  }

  hide() {
    this.showing_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'side-panel': SidePanelElement;
  }
}

customElements.define(SidePanelElement.is, SidePanelElement);
