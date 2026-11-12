// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, repeat} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {IndigoTextOverlayElement} from './text_overlay.js';

export function getHtml(this: IndigoTextOverlayElement) {
  return html`<!--_html_template_start_-->
<div id="container">
  ${repeat(this.showIcon_ ? [this.sequenceId_] : [], id => id, () => html`
    <div id="iconStack" aria-hidden="true">
      <div id="icon" class="${this.useLottie_ ? 'hidden' : ''}"></div>
      <cr-lottie id="lottie" class="${this.useLottie_ ? 'visible' : ''}"
          animation-url="loading_indicator.json"
          @cr-lottie-initialized="${this.onLottieCrLottieInitialized_}">
      </cr-lottie>
    </div>
  `)}
  ${this.currentStep_ !== 0 ? html`
    <div id="steps">
      <div class="${this.getStepClass_(1)}">$i18n{textLayerStep1}</div>
      <div class="${this.getStepClass_(2)}">$i18n{textLayerStep2}</div>
      <div class="${this.getStepClass_(3)}">$i18n{textLayerStep3}</div>
    </div>
    <div id="disclaimer">
      $i18n{disclaimerLine1}
      <br>
      $i18n{disclaimerLine2}
    </div>
  ` : ''}
</div>
<!--_html_template_end_-->`;
}
