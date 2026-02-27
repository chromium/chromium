// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SidePanelElement} from './side_panel.js';

export function getHtml(this: SidePanelElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.showing_ ? html`
  <div id="frame">
    <div id="header">
      <h2>${this.title_}</h2>
      <cr-icon-button id="closeButton" iron-icon="cr:clear"
        @click="${this.onCloseClick_}">
      </cr-icon-button>
    </div>
    <div id="content">${this.webView}</div>
  </div>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
