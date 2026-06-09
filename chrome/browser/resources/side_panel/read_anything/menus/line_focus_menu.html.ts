// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LineFocusMenuElement} from './line_focus_menu.js';

export function getHtml(this: LineFocusMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<grouped-action-menu
    id="menu"
    label="$i18n{lineFocusLabel}"
    .menuGroups="${this.groups_}"
    .nonModal="${this.nonModal}"
    .closeOnClick="${false}"
    @line-focus-style-change="${this.onLineFocusStyleChange_}"
    @line-focus-movement-change="${this.onLineFocusMovementChange_}">
</grouped-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}
