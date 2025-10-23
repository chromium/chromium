// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SetupListItemElement} from './setup_list_item.js';

export function getHtml(this: SetupListItemElement) {
  return html`<!--_html_template_start_-->
<button id="backing" @click="${this.onClick_}"
    class="${this.completed ? 'completed' : 'pending'}"
    ?disabled="${this.completed}"
    aria-label="${this.actionButtonText}" ?aria-disabled="${this.completed}"
    >
  <cr-icon id="bodyIcon" class="${this.completed ? 'completed' : ''}"
    icon="${this.completed ? 'cr:check' : 'ntp-promo:' + this.bodyIconName}">
  </cr-icon>
  <p id="bodyText">${this.bodyText}</p>
  <cr-icon id="actionIcon" icon="cr:chevron-right"></cr-icon>
</button>
<!--_html_template_end_-->`;
}
