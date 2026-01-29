// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import type {ContextualTasksErrorDialogElement} from './error_dialog.js';

// clang-format off
export function getHtml(this: ContextualTasksErrorDialogElement) {
  return html`<!--_html_template_start_-->
  <cr-dialog id="dialog" no-cancel>
    <div slot="title">${loadTimeData.getString('oauthErrorDialogTitle')}</div>
    <div slot="body">${loadTimeData.getString('oauthErrorDialogBody')}</div>
    <div slot="button-container">
      <cr-button class="action-button" @click="${this.onReloadClick_}">
        ${loadTimeData.getString('oauthErrorDialogReloadButton')}
      </cr-button>
    </div>
  </cr-dialog>
  <!--_html_template_end_-->`;
}
// clang-format on
