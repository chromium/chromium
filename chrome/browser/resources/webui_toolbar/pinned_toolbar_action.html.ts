// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {PinnedToolbarActionElement} from './pinned_toolbar_action.js';

export function getHtml(this: PinnedToolbarActionElement) {
  return html`<!--_html_template_start_-->
<toolbar-chip-button id="button"
    class="${this.hasHelpBubble ? 'help-anchor-highlight' : ''}"
    .disabled="${!this.state.enabled}"
    ?is-menu-open="${this.state.highlighted || this.trackedHighlighted}"
    ?is-activated="${this.state.activated}" .tooltip="${this.getTooltip_()}"
    .ariaLabel="${this.state.accessibilityText || this.state.tooltip}"
    .ariaPressed="${
        this.state.highlighted || this.trackedHighlighted ? 'true' : 'false'}"
    draggable="${this.isDraggable()}" @dragstart="${this.onDragstart}"
    @dragend="${this.onDragend}" @keydown="${this.onKeydown}"
    @click="${this.onActionClick_}"
    @pointerdown="${this.highlightTracker.onPointerdown}"
    @contextmenu="${this.onContextmenu_}">
  <icon-from-table slot="prefix-icon" .iconHandle="${this.state.icon}">
  </icon-from-table>
</toolbar-chip-button>
${this.getProgressRingStatus_() ? html`
  <svg class="progress-ring" aria-hidden="true"
      status="${this.getProgressRingStatus_()}"
      ?disabled="${!this.state.enabled}"
      ?is-activated="${this.state.activated}">
    <circle class="track" pathLength="100"></circle>
    <circle class="fill" pathLength="100"
        style="${this.getProgressRingFillStyle_() ?? nothing}">
    </circle>
  </svg>
` : nothing}
<div class="status-indicator" ?hidden="${!this.state.activated}"></div>
${this.getBadgeText_() ? html`
  <div class="badge" aria-hidden="true" ?disabled="${!this.state.enabled}"
      ?dormant="${this.isDormant_()}" ?is-activated="${this.state.activated}">
    ${this.getBadgeText_()}
  </div>
` : nothing}
<!--_html_template_end_-->`;
}
