// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {InstallWarningsDialogElement} from './install_warnings_dialog.js';

export function getHtml(this: InstallWarningsDialogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" show-on-attach>
  <div slot="title">$i18n{installWarnings}</div>
  <div slot="body">
    <ul>
      ${this.installWarnings.map(item => html`<li>${item}</li>`)}
    </ul>
  </div>
  <div slot="button-container">
    <cr-button class="action-button" @click="${this.onOkClick_}">
      $i18n{ok}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
