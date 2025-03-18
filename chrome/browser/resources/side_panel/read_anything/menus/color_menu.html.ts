// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolbarEvent} from '../common.js';

import type {ColorMenuElement} from './color_menu.js';

export function getHtml(this: ColorMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<simple-action-menu
    id="menu"
    label="$i18n{themeTitle}"
    .menuItems="${this.options_}"
    event-name="${ToolbarEvent.THEME}"
    current-selected-index="${this.restoredThemeIndex_()}"
    @theme-change="${this.onThemeChange_}">
</simple-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}
