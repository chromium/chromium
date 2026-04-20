// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {PinnedToolbarActionElement} from './pinned_toolbar_action.js';

export function getHtml(this: PinnedToolbarActionElement) {
  return html`<!--_html_template_start_-->
  <cr-icon-button iron-icon="${this.getIcon_().ironIcon ?? nothing}"
      class="${this.getIcon_().className ?? nothing}"
      ?disabled="${!this.state.enabled}"
      ?is-menu-open="${this.state.highlighted}"
      title="${this.state.tooltip}"
      aria-label="${this.state.accessibilityText || this.state.tooltip}"
      @click="${this.onActionClick_}">
  </cr-icon-button>
<!--_html_template_end_-->`;
}
