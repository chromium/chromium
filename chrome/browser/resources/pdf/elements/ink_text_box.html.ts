// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {InkTextBoxElement} from './ink_text_box.js';

export function getHtml(this: InkTextBoxElement) {
  return html`<!--_html_template_start_-->
    <!-- TODO(crbug.com/414858397): Add labels for screenreaders -->
    <textarea id="textbox" .value="${this.textValue_}" rows="1"
        @input="${this.onTextValueInput_}">
    </textarea>
    <div class="handle top left" tabindex="0"></div>
    <div class="handle top center" tabindex="0"></div>
    <div class="handle top right" tabindex="0"></div>
    <div class="handle left center" tabindex="0"></div>
    <div class="handle right center" tabindex="0"></div>
    <div class="handle bottom left" tabindex="0"></div>
    <div class="handle bottom center" tabindex="0"></div>
    <div class="handle bottom right" tabindex="0"></div>
  <!--_html_template_end_-->`;
}
