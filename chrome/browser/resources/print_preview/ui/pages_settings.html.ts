// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PagesSettingsElement} from './pages_settings.js';
import {PagesValue} from './pages_settings.js';

export function getHtml(this: PagesSettingsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<print-preview-settings-section>
  <span slot="title" id="pages-label">$i18n{pagesLabel}</span>
  <div slot="controls">
    <select class="md-select" aria-labelledby="pages-label"
        ?disabled="${this.controlsDisabled_}" .value="${this.selectedValue}"
        @change="${this.onSelectChange}" @blur="${this.onSelectBlur_}">
      <option value="${PagesValue.ALL}" selected>
        $i18n{optionAllPages}
      </option>
      <option value="${PagesValue.ODDS}"
          ?hidden="${this.isSinglePage_()}">
        $i18n{optionOddPages}
      </option>
      <option value="${PagesValue.EVENS}"
          ?hidden="${this.isSinglePage_()}">
        $i18n{optionEvenPages}
      </option>
      <option value="${PagesValue.CUSTOM}">
        $i18n{optionCustomPages}
      </option>
    </select>
  </div>
</print-preview-settings-section>
<cr-collapse ?opened="${this.shouldShowInput_()}"
    @transitionend="${this.onCollapseChanged_}">
  <print-preview-settings-section id="customInputWrapper">
    <div slot="title"></div>
    <div slot="controls">
      <cr-input id="pageSettingsCustomInput" class="stroked" type="text"
          data-timeout-delay="500" ?invalid="${this.hasError_}"
          ?disabled="${this.inputDisabled_()}"
          spellcheck="false" placeholder="$i18n{examplePageRangeText}"
          error-message="${this.getHintMessage_()}"
          @blur="${this.onCustomInputBlur_}">
      </cr-input>
    </div>
  </print-preview-settings-section>
</cr-collapse><!--_html_template_end_-->`;
  // clang-format on
}
