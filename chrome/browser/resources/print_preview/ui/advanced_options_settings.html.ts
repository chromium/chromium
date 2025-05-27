// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AdvancedOptionsSettingsElement} from './advanced_options_settings.js';

export function getHtml(this: AdvancedOptionsSettingsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<print-preview-settings-section>
  <span slot="title"></span>
  <div slot="controls">
    <cr-button id="button" ?disabled="${this.disabled}"
        @click="${this.onButtonClick_}">
      $i18n{newShowAdvancedOptions}
    </cr-button>
  </div>
</print-preview-settings-section>
${this.showAdvancedDialog_ ? html`
  <print-preview-advanced-settings-dialog
      .destination="${this.destination}" @close="${this.onDialogClose_}">
  </print-preview-advanced-settings-dialog>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
