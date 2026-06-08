// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/icons.html.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AvatarButtonElement} from './avatar_button.js';

export function getHtml(this: AvatarButtonElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-button id="button"
    title="${this.getTooltip_()}"
    aria-label="${this.state.accessibilityName || ''}"
    aria-description="${this.state.accessibilityDescription || ''}"
    @click="${this.onClick_}">
  ${this.state.iconUrl ? html`
    <img id="icon" src="${this.state.iconUrl}" alt="">
  ` : html`
    <cr-icon id="icon" icon="cr:person"></cr-icon>
  `}
  ${this.state.text ? html`
    <span id="text">${this.state.text}</span>
  ` : ''}
</cr-button>
<!--_html_template_end_-->`;
  // clang-format on
}
