// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerDownloadControlsElement} from './viewer_download_controls.js';

export function getHtml(this: ViewerDownloadControlsElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button id="download" iron-icon="cr:file-download"
    @click="${this.onDownloadClick_}" aria-label="$i18n{tooltipDownload}"
    aria-haspopup="${this.downloadHasPopup_()}"
    title="$i18n{tooltipDownload}"></cr-icon-button>
<cr-action-menu id="menu" @open-changed="${this.onOpenChanged_}">
  <button id="download-edited" class="dropdown-item"
      @click="${this.onDownloadEditedClick_}">
    $i18n{downloadEdited}
  </button>
  <button id="download-original" class="dropdown-item"
      @click="${this.onDownloadOriginalClick_}">
    $i18n{downloadOriginal}
  </button>
</cr-action-menu>
<!--_html_template_end_-->`;
}
