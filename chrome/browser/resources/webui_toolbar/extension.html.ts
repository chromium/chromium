// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionElement} from './extension.js';

export function getHtml(this: ExtensionElement) {
  return html`<!--_html_template_start_-->
  <cr-button type="button"
      ?is-menu-open="${this.trackedHighlighted}"
      title="${this.state.tooltip}"
      aria-label="${this.state.accessibleName || this.state.tooltip}"
      @click="${this.onClick_}"
      @contextmenu="${this.onContextmenu_}">
      <icon-from-table .iconHandle="${this.state.icon}"></icon-from-table>
  </cr-button>
<!--_html_template_end_-->`;
}
