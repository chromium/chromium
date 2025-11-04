// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolbarEvent} from '../shared/common.js';

import type {RateMenuElement} from './rate_menu.js';

export function getHtml(this: RateMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<simple-action-menu
    id="menu"
    label="$i18n{voiceSpeedLabel}"
    .menuItems="${this.options_}"
    event-name="${ToolbarEvent.RATE}"
    current-selected-index="${this.restoredRateIndex_()}"
    @rate-change="${this.onRateChange_}">
</simple-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}
