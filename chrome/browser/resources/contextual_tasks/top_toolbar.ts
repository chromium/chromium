// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import {getCss} from './top_toolbar.css.js';
import {getHtml} from './top_toolbar.html.js';

export class TopToolbarElement extends CrLitElement {
  static get is() {
    return 'top-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      title: {type: String},
    };
  }

  override accessor title: string = '';
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override render() {
    return getHtml.bind(this)();
  }

  protected onSigninClick_() {
    this.fire('signin-click');
  }

  protected onCloseButtonClick_() {
    this.browserProxy_.handler.closeSidePanel();
  }

  protected onNewThreadClick_() {
    this.fire('new-thread-click');
  }

  protected onThreadHistoryClick_() {
    this.fire('thread-history-click');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'top-toolbar': TopToolbarElement;
  }
}

customElements.define(TopToolbarElement.is, TopToolbarElement);
