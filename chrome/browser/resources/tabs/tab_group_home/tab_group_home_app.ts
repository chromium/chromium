// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './tab_group_home_app.css.js';
import {getHtml} from './tab_group_home_app.html.js';

export class TabGroupHomeAppElement extends CrLitElement {
  static get is() {
    return 'tab-group-home-app';
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
    'tab-group-home-app': TabGroupHomeAppElement;
  }
}

customElements.define(TabGroupHomeAppElement.is, TabGroupHomeAppElement);
