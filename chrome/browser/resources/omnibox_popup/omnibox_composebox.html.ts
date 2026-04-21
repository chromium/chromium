// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxComposeboxElement} from './omnibox_composebox.js';

export function getHtml(this: OmniboxComposeboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <div id="composebox" part="composebox" ?inert="${!!this.errorMessage}">
      <div id="inputContainer" part="input-container">
        <!-- TODO(crbug.com/486706573): Add back cancel button title and cancel click handler once added to mixin. -->
        <cr-composebox-input id="composeboxInput"
            exportparts="text-container, icon-container, mirror, input, smart-compose, cancel, action-icon, cancel-icon"
            .disableCaretColorAnimation="${this.disableCaretColorAnimation}"
            .showDropdown="${this.showDropdown}"
            .inputPlaceholder="${this.inputPlaceholder}"
            .input="${this.input}"
            .smartComposeInlineHint="${this.smartComposeInlineHint}"
            .submitEnabled="${this.submitEnabled}"
            .entrypointName="${this.entrypointName}"
            @input-input="${this.onInputInput}"
            @input-focusin="${this.onInputFocusin}">
        </cr-composebox-input>
      </div>
    </div>
<!--_html_template_end_-->`;
  // clang-format on
}
