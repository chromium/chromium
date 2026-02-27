// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {DataExportDoneElement} from './data_export_done.js';

export function getHtml(this: DataExportDoneElement) {
  // clang-format off
  return html`<!--html_template_start_-->
<h1 tabindex="0">${this.i18n('dataExportDonePageTitle')}</h1>
<div class="support-tool-title" tabindex="0">
  ${this.i18n('dataExportedText')}
</div>
<div>
  <cr-icon id="check-icon" icon="cr:check-circle"></cr-icon>
  <a id="path-link" @click="${this.onFilePathClick_}" href="#"
      is="action-link" tabindex="0">
    ${this.path_}
  </a>
</div>
  <!--html_template_end_-->`;
  // clang-format on
}
