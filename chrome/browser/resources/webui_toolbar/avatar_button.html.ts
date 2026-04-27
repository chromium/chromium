// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AvatarButtonElement} from './avatar_button.js';

export function getHtml(this: AvatarButtonElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-icon-button id="button"
    ?hidden="${!this.state?.isVisible}"
    .ironIcon="${this.getIcon()}"
    title="${this.state?.accessibilityDescription || ''}"
    aria-label="${this.state?.accessibilityName || ''}"
    ?disabled="${!!this.state?.isButtonActionDisabled}"
    @click="${this.onClick_}">
</cr-icon-button>
<!--_html_template_end_-->`;
  // clang-format on
}
