// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolbarEvent} from '../common.js';

import type {HighlightMenuElement} from './highlight_menu.js';

export function getHtml(this: HighlightMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<simple-action-menu
    id="menu"
    label="$i18n{voiceHighlightLabel}"
    .menuItems="${this.options_}"
    event-name="${ToolbarEvent.HIGHLIGHT_CHANGE}"
    current-selected-index="${this.restoredHighlightIndex_()}">
</simple-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}
