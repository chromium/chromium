// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LanguageToastElement} from './language_toast.js';

export function getHtml(this: LanguageToastElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-toast id="toast" duration="${this.toastDuration_}">
  <div id="toastDiv">
    <span id="toastTitle">${this.toastTitle_}</span>
    ${this.toastMessage_ ? html
        `<span id="toastMessage">${this.toastMessage_}</span>`
        : ''
    }
  </div>
</cr-toast>
<!--_html_template_end_-->`;
  // clang-format on
}
