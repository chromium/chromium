// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="webnn_enable_graph_dump">
import './graph_dump.js';
// </if>
import '//resources/cr_elements/cr_page_selector/cr_page_selector.js';
import '//resources/cr_elements/cr_tabs/cr_tabs.js';

import type {CrTabsElement} from '//resources/cr_elements/cr_tabs/cr_tabs.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export interface WebnnInternalsAppElement {
  $: {
    tabs: CrTabsElement,
  };
}

export class WebnnInternalsAppElement extends CrLitElement {
  static get is() {
    return 'webnn-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedTabIndex_: {type: Number},
    };
  }

  protected accessor selectedTabIndex_: number = 0;

  protected onSelectedChanged_(e: CustomEvent<{value: number}>) {
    this.selectedTabIndex_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webnn-internals-app': WebnnInternalsAppElement;
  }
}

customElements.define(WebnnInternalsAppElement.is, WebnnInternalsAppElement);
