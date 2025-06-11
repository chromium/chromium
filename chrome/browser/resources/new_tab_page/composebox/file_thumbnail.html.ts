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
    <img class="thumbnail" src="${this.file.objectUrl}">
    </img>` : html`
    <!-- TODO(): Replace with icon for file type -->
    <p class="thumbnail">${this.file.name}</p>
  `}
  <cr-icon-button id="deleteButton"
      iron-icon="cr:delete"
      @click=${this.deleteFile_}>
  </cr-icon-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
