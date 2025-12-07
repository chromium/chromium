// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerAttachmentBarElement} from './viewer_attachment_bar.js';

export function getHtml(this: ViewerAttachmentBarElement) {
  return html`<!--_html_template_start_-->
<div id="warning" ?hidden="${!this.exceedSizeLimit_()}">
  $i18n{oversizeAttachmentWarning}
</div>
${this.attachments.map((item, index) => html`
  <viewer-attachment .attachment="${item}" .index="${index}">
  </viewer-attachment>`)}
<!--_html_template_end_-->`;
}
