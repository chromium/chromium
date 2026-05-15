// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NtpComposeboxElement} from './ntp_composebox.js';

export function getHtml(this: NtpComposeboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <div id="composebox" part="composebox">
      <div id="inputContainer" part="input-container">
        <cr-composebox-input id="composeboxInput"
            exportparts="text-container, icon-container, mirror, input, smart-compose, cancel, action-icon, cancel-icon"
            .showDropdown="${this.showDropdown}"
            .inputPlaceholder="${this.inputPlaceholder}"
            .input="${this.input}"
            .smartComposeEnabled="${this.smartComposeEnabled}"
            .smartComposeInlineHint="${this.smartComposeInlineHint}"
            .submitEnabled="${this.submitEnabled}"
            .cancelButtonTitle="${this.computeCancelButtonTitle_()}"
            @input-input="${this.onInputInput}"
            @input-focusin="${this.onInputFocusin}"
            @cancel-click="${this.onCancelClick}">
        </cr-composebox-input>
      </div>
    </div>
  <!--_html_template_end_-->`;
  // clang-format on
}
