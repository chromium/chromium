// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {IndigoTextOverlayElement} from './text_overlay.js';

export function getHtml(this: IndigoTextOverlayElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container">
  ${this.showIcon_ ? html`<div id="icon"></div>` : ''}
  ${this.currentStep_ !== 0 ? html`
    <div id="steps">
      <div class="${this.getStepClass_(1)}">$i18n{textLayerStep1}</div>
      <div class="${this.getStepClass_(2)}">$i18n{textLayerStep2}</div>
      <div class="${this.getStepClass_(3)}">$i18n{textLayerStep3}</div>
    </div>
    <div id="disclaimer">
      $i18n{disclaimerLine1}<br>
      $i18n{disclaimerLine2}
    </div>
  ` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
