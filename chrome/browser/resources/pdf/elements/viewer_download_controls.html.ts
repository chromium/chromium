// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerDownloadControlsElement} from './viewer_download_controls.js';

export function getHtml(this: ViewerDownloadControlsElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button id="save" iron-icon="cr:file-download" part="button"
    @click="${this.onSaveClick}" aria-label="$i18n{tooltipDownload}"
    aria-haspopup="${this.getAriaHasPopup()}"
    title="$i18n{tooltipDownload}"></cr-icon-button>
<cr-action-menu id="menu">
  <button id="save-edited" class="dropdown-item"
      @click="${this.onSaveEditedClick}">
    $i18n{downloadEdited}
  </button>
  <button id="save-original" class="dropdown-item"
      @click="${this.onSaveOriginalClick}">
    $i18n{downloadOriginal}
  </button>
</cr-action-menu>
<!--_html_template_end_-->`;
}
