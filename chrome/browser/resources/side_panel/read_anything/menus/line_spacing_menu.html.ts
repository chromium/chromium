// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolbarEvent} from '../shared/common.js';

import type {LineSpacingMenuElement} from './line_spacing_menu.js';

export function getHtml(this: LineSpacingMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<simple-action-menu
    id="menu"
    label="$i18n{lineSpacingTitle}"
    event-name="${ToolbarEvent.LINE_SPACING}"
    .menuItems="${this.options_}"
    current-selected-index="${this.restoredLineSpacingIndex_()}"
    @line-spacing-change="${this.onLineSpacingChange_}">
</simple-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}
