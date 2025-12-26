// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolbarEvent} from '../content/read_anything_types.js';

import type {LineFocusMenuElement} from './line_focus_menu.js';

export function getHtml(this: LineFocusMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<simple-action-menu
    id="menu"
    label="$i18n{lineFocusLabel}"
    .menuItems="${this.options_}"
    .nonModal="${this.nonModal}"
    event-name="${ToolbarEvent.LINE_FOCUS}"
    current-selected-index="${this.restoredLineFocusIndex_()}"
    @line-focus-change="${this.onLineFocusChange_}">
</simple-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}
