// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {InkTextBoxElement} from './ink_text_box.js';

export function getHtml(this: InkTextBoxElement) {
  return html`<!--_html_template_start_-->
    <div class="handle top left"></div>
    <div class="handle top center"></div>
    <div class="handle top right"></div>
    <div class="handle left center"></div>
    <div class="handle right center"></div>
    <div class="handle bottom left"></div>
    <div class="handle bottom center"></div>
    <div class="handle bottom right"></div>
    <textarea id="textbox" .value="${this.textValue_}" rows="1"
        @input="${this.onTextValueInput_}"
        @focus="${this.onTextareaFocus_}">
    </textarea>
  <!--_html_template_end_-->`;
}
