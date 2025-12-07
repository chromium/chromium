// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolbarEvent} from '../shared/common.js';

import type {FontMenuElement} from './font_menu.js';

export function getHtml(this: FontMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<simple-action-menu
    id="menu"
    label="$i18n{fontNameTitle}"
    .menuItems="${this.options_}"
    event-name="${ToolbarEvent.FONT}"
    current-selected-index="${this.currentFontIndex_()}"
    @font-change="${this.onFontChange_}">
</simple-action-menu>
  <!--_html_template_end_-->`;
  // clang-format on
}
