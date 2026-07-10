// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppearanceMenuElement} from './appearance_menu.js';

export function getHtml(this: AppearanceMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<grouped-action-menu
    id="menu"
    label="$i18n{appearanceTitle}"
    .menuGroups="${this.groups_}"
    .nonModal="${this.nonModal}"
    .closeOnClick="${false}"
    @theme-change="${this.onThemeChange_}"
    @presentation-change="${this.onPresentationChange_}">
</grouped-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}
