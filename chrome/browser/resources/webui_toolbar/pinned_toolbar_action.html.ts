// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {PinnedToolbarActionElement} from './pinned_toolbar_action.js';

export function getHtml(this: PinnedToolbarActionElement) {
  return html`<!--_html_template_start_-->
  <cr-icon-button iron-icon="${this.getIronIcon_() ?? nothing}"
      style="${this.getIconStyle_() ?? nothing}"
      ?disabled="${!this.state.enabled}"
      ?is-menu-open="${this.state.highlighted || this.trackedHighlighted}"
      ?is-activated="${this.state.activated}"
      title="${this.getTooltip_()}"
      aria-label="${this.state.accessibilityText || this.state.tooltip}"
      @click="${this.onActionClick_}"
      @contextmenu="${this.onContextmenu_}">
  </cr-icon-button>
<div class="status-indicator" ?hidden="${!this.state.activated}"></div>
<!--_html_template_end_-->`;
}
