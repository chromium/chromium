// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ToolbarEvent} from '../content/read_anything_types.js';

import type {PresentationMenuElement} from './presentation_menu.js';

export function getHtml(this: PresentationMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<simple-action-menu
    non-modal
    id="menu"
    label="$i18n{viewLabel}"
    .menuItems="${this.options_}"
    event-name="${ToolbarEvent.PRESENTATION_CHANGE}"
    current-selected-index="${this.restoredPresentationIndex_()}"
    @presentation-change="${this.onPresentationChange_}">
</simple-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}
