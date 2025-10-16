// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import '//resources/cr_components/composebox/composebox.js';

export class ContextualTasksAppElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-app';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      threadUrl_: {type: String},
    };
  }

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  protected accessor threadUrl_: string = '';

  override async connectedCallback() {
    super.connectedCallback();

    const {url} = await this.browserProxy_.getThreadUrl();
    this.threadUrl_ = url.url;

    // Tell the browser the WebUI is loaded and ready to show in side panel. If
    // the WebUI is loadded in a tab it's an no-op.
    this.browserProxy_.showUi();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-app': ContextualTasksAppElement;
  }
}

customElements.define(ContextualTasksAppElement.is, ContextualTasksAppElement);
