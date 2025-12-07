// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TabElement} from './tab_element.js';

export function getHtml(this: TabElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="tab${this.active ? ' active' : ''}"
     @click="${this.handleClick}">
  <div id="faviconContainer">
    <div id="favicon"></div>
  </div>
  <span id="title">${this.tabTitle}</span>
  <cr-icon-button
    class="close"
    iron-icon="cr:clear"
    @click="${this.handleClose}"></cr-icon-button>
  <div id="bottomCorners" ?hidden="${!this.active}">
    <div id="leftCorner" class="corner"></div>
    <div id="rightCorner" class="corner"></div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
