// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TextMenuElement} from './text_menu.js';

export function getHtml(this: TextMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<grouped-action-menu
    id="menu"
    label="$i18n{textSettingsTitle}"
    .menuGroups="${this.groups_}"
    .nonModal="${this.nonModal}"
    .closeOnClick="${false}"
    @font-change="${this.onFontChange_}"
    @line-spacing-change="${this.onLineSpacingChange_}"
    @letter-spacing-change="${this.onLetterSpacingChange_}"
    @line-focus-style-change="${this.onLineFocusStyleChange_}"
    @line-focus-toggle-change="${this.onLineFocusToggleChange_}"
    @line-focus-movement-change="${this.onLineFocusMovementChange_}">
</grouped-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}
