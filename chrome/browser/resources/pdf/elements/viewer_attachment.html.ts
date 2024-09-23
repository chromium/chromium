// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerAttachmentElement} from './viewer_attachment.js';

export function getHtml(this: ViewerAttachmentElement) {
  return html`<!--_html_template_start_-->
<div id="item">
  <span id="title">${this.attachment.name}</span>
  <cr-icon-button id="download" tabindex="0" ?hidden="${!this.saveAllowed_}"
      title="$i18n{tooltipDownloadAttachment}" iron-icon="cr:file-download"
      @click="${this.onDownloadClick_}">
  </cr-icon-button>
</div>
<!--_html_template_end_-->`;
}
