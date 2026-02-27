// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PasscodeInputElement} from './passcode_input.js';

export function getHtml(this: PasscodeInputElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container">
  <input id="inputElement" aria-label="${this.ariaLabel}" autocomplete="off"
      class="hidden-input" .maxLength="${this.length}"
      ?disabled="${this.disabled}" spellcheck="false"
      type="text" @blur="${this.handleOnBlur}" @click="${this.renderSelection}"
      @keyup="${this.renderSelection}" @select="${this.renderSelection}"
      @input="${this.handleOnInput}" @focus="${this.handleOnFocus}">
  <div class="char-box-container" aria-hidden="true">
    ${this.charDisplayBoxes.map((item, index) => html`
      <div id="char-box-${index}"
          class="char-box ${this.getCharBoxClass(index)}">
        <div class="char-box-focus">
          <p id="char-${index}" class="char ${this.getDisabledClass()}">
            ${item}
          </p>
        </div>
      </div>
    `)}
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
