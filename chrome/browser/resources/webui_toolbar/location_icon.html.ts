// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LocationIconElement} from './location_icon.js';

export function getHtml(this: LocationIconElement) {
  // Note: `has-label` is always set because this element manages the label's
  // visibility and expand animation itself (see #text in location_icon.css),
  // so toolbar-chip-button must not collapse the label on its own.
  // TODO(crbug.com/542668195): Stop disabling the pressed highlight while
  // GlowUp is active. Native LocationIconView::MaybeAnimateIcon() doesn't touch
  // the ink drop, so native still shows the pressed ripple during GlowUp.
  return html`<!--_html_template_start_-->
<toolbar-chip-button id="button" has-label outset-focus-ring
    class="${this.hasHelpBubble ? 'help-anchor-highlight' : ''}"
    ?disable-hover-highlight="${!this.clickable}"
    ?disable-pressed-highlight="${!this.clickable || this.glowUpActive}"
    .buttonTabIndex="${this.clickable ? 0 : -1}"
    .buttonRole="${this.getAccessibilityRole_()}"
    .tooltip="${this.state.tooltip}"
    .ariaLabel="${this.state.accessibilityState.label}"
    .ariaDescription="${this.state.accessibilityState.description}"
    @pointerdown="${this.onPointerdown_}" @click="${this.onClick_}"
    @auxclick="${this.onAuxclick_}" @contextmenu="${this.onContextmenu_}">
  ${this.glowUpActive ? html`
    <cr-icon id="icon" slot="prefix-icon"
        icon="${this.state.isContextMenuVisible ?
            'webui-toolbar:info_glow_up_forward' :
            'webui-toolbar:info_glow_up_reverse'}">
    </cr-icon>
  ` : html`
    <icon-from-table slot="prefix-icon" .iconHandle="${this.state.icon}">
    </icon-from-table>
  `}
  <span id="text">${this.displayText}</span>
</toolbar-chip-button>
<!--_html_template_end_-->`;
}
