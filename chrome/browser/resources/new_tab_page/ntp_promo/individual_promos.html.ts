// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {IndividualPromosElement} from './individual_promos.js';

export function getHtml(this: IndividualPromosElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="promos">
  ${this.promo_ ? html`
    <button id="promo" @click="${this.onClick_.bind(this, this.promo_.id)}"
        aria-label="${this.promo_.buttonText}">
      <cr-icon id="bodyIcon" icon="ntp-promo:${this.promo_.iconName}"></cr-icon>
      <p id="bodyText" class="${this.getBodyTextCssClass_()}">
        ${this.promo_.bodyText}
      </p>
      <cr-icon id="actionIcon" icon="cr:chevron-right"></cr-icon>
    </button>` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
