// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import {WebviewElement} from './webview.js';

export class SidePanel extends CrLitElement {
  static get is() {
    return 'side-panel';
  }

  private webView_?: WebviewElement;
  private showing_: boolean = false;

  override render() {
    if (!this.showing_) {
      return nothing;
    }

    return this.webView_;
  }

  show(guestContentsId: number) {
    this.webView_ = new WebviewElement();
    this.webView_.guestId = guestContentsId;
    this.showing_ = true;
    this.requestUpdate();
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
