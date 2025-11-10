// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FontSelectElement} from './font_select.js';

export function getHtml(this: FontSelectElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<select id="select" class="md-select" tabindex="-1"
    @change="${this.onFontSelectValueChange_}"
    aria-label="$i18n{fontNameTitle}"
    title="$i18n{fontNameTitle}">
  ${this.options.map((item) => html`
    <option value="${item.data}">
      ${this.getFontItemLabel_(item.data)}
    </option>
  `)}
</select>
  <!--_html_template_end_-->`;
  // clang-format on
}
