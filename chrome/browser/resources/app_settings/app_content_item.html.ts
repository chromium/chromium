// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppContentItemElement} from './app_content_item.js';

export function getHtml(this: AppContentItemElement) {
  return html`<!--_html_template_start_-->
<cr-link-row
    id="appContent"
    label="$i18n{appManagementAppContentLabel}"
    sub-label="$i18n{appManagementAppContentSublabel}"
    @click="${this.onAppContentClick_}">
</cr-link-row>
${this.showAppContentDialog ? html`
  <app-management-app-content-dialog id="appContentDialog" .app="${this.app}"
      @close="${this.onAppContentDialogClose_}">
  </app-management-app-content-dialog>
` : ''}
<!--_html_template_end_-->`;
}
