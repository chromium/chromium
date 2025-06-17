// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons.css.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxElement} from './composebox.js';

export function getHtml(this: ComposeboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <div id="composebox">
    <div id="inputContainer">
      <textarea id="input" type="search" autocomplete="off"
          spellcheck="false" placeholder="Ask Anything"></textarea>
      <div id="uploadContainer">
        <cr-icon-button
            class="upload-icon no-overlap"
            id="imageUploadButton"
            iron-icon="composebox:imageUpload"
            title="$i18n{composeboxImageUploadButtonTitle}"
            @click="${this.openImageUpload_}">
        </cr-icon-button>
        <cr-icon-button
            class="upload-icon no-overlap"
            id="fileUploadButton"
            iron-icon="composebox:fileUpload"
            title="$i18n{composeboxFileUploadButtonTitle}"
            @click="${this.openFileUpload_}">
        </cr-icon-button>
      </div>
    </div>
  </div>
  <ntp-composebox-file-carousel
      id="carousel"
      .files=${this.files_}
      @delete-file=${this.onDeleteFile_}>
  </ntp-composebox-file-carousel>
  <input type="file"
      accept="${this.imageFileTypes_}"
      id="imageInput"
      @change="${this.onFileChange_}"
      hidden>
  </input>
  <input type="file"
      accept="${this.attachmentFileTypes_}"
      id="fileInput"
      @change="${this.onFileChange_}"
      hidden>
  </input>
<!--_html_template_end_-->`;
  // clang-format on
}
