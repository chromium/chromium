// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './search_bubble_app.html.js';
import {SearchBubbleProxyImpl} from './search_bubble_proxy.js';
import type {SearchBubbleProxy} from './search_bubble_proxy.js';

export class SearchBubbleAppElement extends PolymerElement {
  static get is() {
    return 'search-bubble-app';
  }

  static get template() {
    return getTemplate();
  }

  private browserProxy_: SearchBubbleProxy =
      SearchBubbleProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.browserProxy_.handler.showUI();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'search-bubble-app': SearchBubbleAppElement;
  }
}

customElements.define(SearchBubbleAppElement.is, SearchBubbleAppElement);
