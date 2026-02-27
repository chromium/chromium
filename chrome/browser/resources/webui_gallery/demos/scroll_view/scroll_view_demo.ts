// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './scroll_view_demo.css.js';
import {getHtml} from './scroll_view_demo.html.js';

export class ScrollViewDemoElement extends CrLitElement {
  static get is() {
    return 'scroll-view-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'scroll-view-demo': ScrollViewDemoElement;
  }
}

export const tagName = ScrollViewDemoElement.is;

customElements.define(ScrollViewDemoElement.is, ScrollViewDemoElement);
