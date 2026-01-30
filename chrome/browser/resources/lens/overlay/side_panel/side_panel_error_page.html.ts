// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SidePanelErrorPageElement} from './side_panel_error_page.js';

export function getHtml(this: SidePanelErrorPageElement) {
  return html`<div id="errorPage">
  <div id="errorIcon">
    <picture id="genericIcon">
      <source srcset="generic-error-icon-dark.png"
        media="${this.darkMode}">
      <img src="generic-error-icon.png">
    </picture>
    <span id="protectedIcon"></span>
  </div>
  <div id="errorTopLine">${this.getTopLineError_()}</div>
  <div id="errorBottomLine">${this.getBottomLineError_()}</div>
</div>`;
}
