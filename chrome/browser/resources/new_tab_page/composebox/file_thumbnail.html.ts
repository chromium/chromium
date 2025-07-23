// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxFileThumbnailElement} from './file_thumbnail.js';

export function getHtml(this: ComposeboxFileThumbnailElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container">
  ${this.file.objectUrl ? html`
    <div id="imgChip" aria-label="${this.file.name}">
      <img class="img-thumbnail"
           src="${this.file.objectUrl}"></img>
        <cr-icon-button
            class="img-overlay"
            id="removeImgButton"
            iron-icon="cr:clear"
            title="${this.deleteFileButtonTitle}"
            @click="${this.deleteFile_}">
        </cr-icon-button>
    </div>` : html`
    <div id="pdfChip">
      <div id="pdfThumbnail">
        <cr-icon icon="thumbnail:pdf" class="pdf-icon">
        </cr-icon>
        <div class="pdf-overlay">
          <cr-icon-button
              id="removePdfButton"
              iron-icon="cr:clear"
              title="${this.deleteFileButtonTitle}"
              @click="${this.deleteFile_}">
          </cr-icon-button>
        </div>
      </div>
      <p class="pdf-title">${this.file.name}</p>
    </div>
  `}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
