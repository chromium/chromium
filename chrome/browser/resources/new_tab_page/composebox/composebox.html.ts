// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxElement} from './composebox.js';

export function getHtml(this: ComposeboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="main">
  <ntp-composebox-file-carousel id="carousel" .files=${this.files}>
  </ntp-composebox-file-carousel>
  <!-- TODO(crbug.com/422561574): Style inputs. -->
  <label for="imageUploader">Image Upload</label>
  <input type="file"
      accept="${this.imageFileTypes_}"
      id="imageUploader"
      @change="${this.onFileChange_}">
  </input>
  <label for="attachmentUploader">Attachment Upload</label>
  <input type="file"
      accept="${this.attachmentFileTypes_}"
      id="attachmentUploader"
      @change="${this.onFileChange_}">
  </input>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
