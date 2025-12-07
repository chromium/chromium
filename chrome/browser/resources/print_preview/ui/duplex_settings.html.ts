// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {DuplexMode} from '../data/model.js';

import type {DuplexSettingsElement} from './duplex_settings.js';

export function getHtml(this: DuplexSettingsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<print-preview-settings-section>
  <div slot="title">
    <label id="label">$i18n{optionTwoSided}</label>
  </div>
  <div slot="controls" class="checkbox">
    <cr-checkbox id="duplex" aria-labelledby="label"
        ?disabled="${this.getDisabled_(this.duplexManaged_)}"
        @change="${this.onCheckboxChange_}">
      $i18n{printOnBothSidesLabel}
    </cr-checkbox>
  </div>
</print-preview-settings-section>
<cr-collapse ?opened="${this.collapseOpened_}">
  <print-preview-settings-section>
    <div slot="title"></div>
    <div slot="controls">
      <select class="md-select" aria-labelledby="duplex"
          .style="background-image: ${this.backgroundImages_};"
          ?disabled="${this.getDisabled_(this.duplexShortEdgeManaged_)}"
          .value="${this.selectedValue}" @change="${this.onSelectChange}">
        <option value="${DuplexMode.LONG_EDGE}">
          $i18n{optionLongEdge}
        </option>
        <option value="${DuplexMode.SHORT_EDGE}">
          $i18n{optionShortEdge}
        </option>
      </select>
    </div>
  </print-preview-settings-section>
</cr-collapse><!--_html_template_end_-->`;
  // clang-format on
}
