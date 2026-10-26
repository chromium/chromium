// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionElement} from './extension.js';

export function getHtml(this: ExtensionElement) {
  return html`<!--_html_template_start_-->
<toolbar-chip-button id="button"
    class="${this.hasHelpBubble ? 'help-anchor-highlight' : ''}"
    ?is-menu-open="${this.state.highlighted || this.trackedHighlighted}"
    .tooltip="${this.state.tooltip}"
    .ariaLabel="${this.state.accessibleName || this.state.tooltip}"
    .ariaHasPopup="${'menu'}" draggable="${this.isDraggable()}"
    @dragstart="${this.onDragstart}" @dragend="${this.onDragend}"
    @keydown="${this.onKeydown}"
    @pointerdown="${this.highlightTracker.onPointerdown}"
    @click="${this.onClick_}" @contextmenu="${this.onContextmenu_}">
  <icon-from-table slot="prefix-icon" .iconHandle="${this.state.icon}">
  </icon-from-table>
</toolbar-chip-button>
<!--_html_template_end_-->`;
}
