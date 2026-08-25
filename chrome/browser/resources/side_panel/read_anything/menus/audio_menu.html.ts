// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AudioMenuElement} from './audio_menu.js';

export function getHtml(this: AudioMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<grouped-action-menu
    id="menu"
    label="$i18n{audioTitle}"
    .menuGroups="${this.groups_}"
    .nonModal="${this.nonModal}"
    .closeOnClick="${false}"
    @highlight-change="${this.onHighlightChange_}">
</grouped-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}
