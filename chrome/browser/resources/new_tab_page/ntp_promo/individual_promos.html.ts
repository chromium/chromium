// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {IndividualPromosElement} from './individual_promos.js';

export function getHtml(this: IndividualPromosElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="promos">
  ${this.eligiblePromos_.map(item => html`
    <button id="promo" @click="${this.onClick_.bind(this, item.id)}"
        aria-label="${item.buttonText}">
      <cr-icon id="bodyIcon" icon="ntp-promo:${item.iconName}"></cr-icon>
      <p id="bodyText" class="${this.getBodyTextCssClass_()}">
        ${item.bodyText}
      </p>
      <cr-icon id="actionIcon" icon="cr:chevron-right"></cr-icon>
    </button>`)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
