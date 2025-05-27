// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PreviewAreaElement} from './preview_area.js';

export function getHtml(this: PreviewAreaElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="preview-area-overlay-layer ${this.getInvisible_()}"
    aria-hidden="${this.isInDisplayPreviewState_()}">
  <div class="preview-area-message">
    <div>
      <span .innerHTML="${this.currentMessage_()}"></span>
      <span class="preview-area-loading-message-jumping-dots
          ${this.getJumpingDots_()}" ?hidden="${!this.isPreviewLoading_()}">
        <span>.</span><span>.</span><span>.</span>
      </span>
    </div>
  </div>
</div>
<div class="preview-area-plugin-wrapper"></div>
<print-preview-margin-control-container id="marginControlContainer"
    .pageSize="${this.pageSize}" .documentMargins="${this.margins}"
    .measurementSystem="${this.measurementSystem}" state="${this.state}"
    ?preview-loaded="${this.previewLoaded()}"
    @text-focus-position="${this.onTextFocusPosition_}"
    @margin-drag-changed="${this.onMarginDragChanged_}">
</print-preview-margin-control-container>
<!--_html_template_end_-->`;
  // clang-format on
}
