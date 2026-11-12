// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {GlicActorTaskIconElement} from './glic_actor_task_icon.js';

export function getHtml(this: GlicActorTaskIconElement) {
  return html`<!--_html_template_start_-->
<toolbar-chip-button id="button"
    class="${this.hasHelpBubble ? 'help-anchor-highlight' : ''}"
    flat-edge="start" .buttonTabIndex="${0}"
    ?has-label="${this.shouldShowLabel_()}"
    ?is-menu-open="${this.state.isPressed}" .tooltip="${this.getTooltip_()}"
    .ariaLabel="${this.state.tooltip}"
    .ariaExpanded="${this.state.isPressed ? 'true' : 'false'}"
    .ariaHasPopup="${'dialog'}" @pointerdown="${this.onPointerdown_}"
    @click="${this.onClick_}" @keydown="${this.onKeydown_}">
  <cr-icon slot="prefix-icon" id="icon" icon="webui-toolbar:cursor_spark">
  </cr-icon>
  ${this.getLabel_() ? html`
    <span id="text">${this.getLabel_()}</span>
  ` : ''}
</toolbar-chip-button>
<!--_html_template_end_-->`;
}
