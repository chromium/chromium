// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxy} from './browser_proxy.js';
import {getCss} from './side_panel.css.js';
import {getHtml} from './side_panel.html.js';
import {WebviewElement} from './webview.js';

export class SidePanel extends CrLitElement {
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
    };
  }

  accessor webView: WebviewElement|null = null;
  protected accessor title_: string = '';
  protected accessor showing_: boolean = false;

  show(guestContentsId: number, title: string) {
    this.webView = new WebviewElement();
    this.webView.guestId = guestContentsId;
    this.title_ = title;
    this.showing_ = true;
    this.requestUpdate();
  }

  async close() {
    this.showing_ = false;
    this.webView = null;
    this.dispatchEvent(new Event('side-panel-closed', {bubbles: true}));
    await this.updateComplete;
    BrowserProxy.getPageHandler().onSidePanelClosed();
  }

  hide() {
    this.showing_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'side-panel': SidePanel;
  }
}

customElements.define(SidePanel.is, SidePanel);
