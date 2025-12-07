// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MarginControlContainerElement} from './margin_control_container.js';

export function getHtml(this: MarginControlContainerElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.marginSides_.map(item => html`
  <print-preview-margin-control side="${item}" ?invisible="${this.invisible_}"
      ?disabled="${this.controlsDisabled_()}"
      .translateTransform="${this.translateTransform_}"
      .clipSize="${this.clipSize_}"
      .measurementSystem="${this.measurementSystem}"
      .scaleTransform="${this.scaleTransform_}"
      .pageSize="${this.pageSize}"
      @pointerdown="${this.onPointerDown_}"
      @text-change="${this.onTextChange_}" @text-blur="${this.onTextBlur_}"
      @text-focus="${this.onTextFocus_}"
      @transition-end="${this.onTransitionEnd_}">
  </print-preview-margin-control>
`)}
<!--_html_template_end_-->`;
  // clang-format on
}
