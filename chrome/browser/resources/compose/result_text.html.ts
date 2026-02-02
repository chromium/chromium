// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeResultTextElement} from './result_text.js';

export function getHtml(this: ComposeResultTextElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="root">
  <div id="resultText" class="result-text"
      ?hidden="${this.textInput.streamingEnabled}"
      contenteditable="plaintext-only"
      @focusin="${this.onFocusIn_}"
      @focusout="${this.onFocusOut_}"
      aria-label="$i18n{resultText}"></div>
  <div id="partialResultText" class="result-text"
      ?hidden="${!this.textInput.streamingEnabled}"
      contenteditable="${this.partialTextCanEdit_()}"
      @focusin="${this.onFocusIn_}"
      @focusout="${this.onFocusOut_}">${this.displayedChunks_.map(
          item => html`<span>${item.text}</span>`)}</div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
