// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppContentDialogElement} from './app_content_dialog.js';

export function getHtml(this: AppContentDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" close-text="$i18n{close}" show-on-attach
    show-close-button>
  <div slot="title">$i18n{appManagementAppContentLabel}</div>
  <div slot="body">$i18n{appManagementAppContentDialogSublabel}</div>
  <div id="dialogBody" slot="body" scrollable>
    ${this.app.scopeExtensions.map(item => html`
      <div class="list-item">${item}</div>
    `)}
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
