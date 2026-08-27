// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './app.html.js';
import {browserProxyFactory} from './page_action_internals.mojom-webui.js';
import type {PageHandlerInterface} from './page_action_internals.mojom-webui.js';

export class PageActionInternalsAppElement extends CrLitElement {
  static get is() {
    return 'page-action-internals-app';
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected pageHandler_: PageHandlerInterface =
      browserProxyFactory.getInstance().handler;
}

declare global {
  interface HTMLElementTagNameMap {
    'page-action-internals-app': PageActionInternalsAppElement;
  }
}

customElements.define(
    PageActionInternalsAppElement.is, PageActionInternalsAppElement);
