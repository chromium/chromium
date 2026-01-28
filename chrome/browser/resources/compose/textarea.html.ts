// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeTextareaElement} from './textarea.js';

export function getHtml(this: ComposeTextareaElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="inputContainer" ?hidden="${this.readonly}">
  <textarea id="input"
      placeholder="$i18n{inputPlaceholderTitle}
 • $i18n{inputPlaceholderLine1}
 • $i18n{inputPlaceholderLine2}
 • $i18n{inputPlaceholderLine3}"
      .value="${this.value}"
      @input="${this.onInput_}"
      required
      autofocus
      aria-invalid="${this.invalidInput_}"
      aria-errormessage="error"
      @change="${this.onChangeTextArea_}">
  </textarea>
  <div id="error" class="error" role="region" aria-live="assertive"
      ?hidden="${!this.invalidInput_}">
    <div id="tooShortError" ?hidden="${!this.tooShort_}">
      $i18n{errorTooShort}
    </div>
    <div id="tooLongError" ?hidden="${!this.tooLong_}">$i18n{errorTooLong}</div>
  </div>
</div>

<div id="readonlyContainer" ?hidden="${!this.readonly}">
  <div id="readonlyText">${this.value}</div>
  <div id="editButtonContainer"
      ?hidden="${!this.shouldShowEditIcon_()}">
    <cr-icon-button id="editButton" iron-icon="compose:edit"
        title="$i18n{editButton}" @click="${this.onEditClick_}">
    </cr-icon-button>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
