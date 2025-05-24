// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './selectable_icon_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {BRUSH_TYPES} from './ink_brush_selector.js';
import type {InkBrushSelectorElement} from './ink_brush_selector.js';

export function getHtml(this: InkBrushSelectorElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <cr-radio-group selectable-elements="selectable-icon-button"
        .selected="${this.currentType}" aria-label="$i18n{ink2Tool}"
        @selected-changed="${this.onSelectedChanged_}">
      ${BRUSH_TYPES.map(brush =>  html`
        <selectable-icon-button id="${brush}"
            icon="${this.getIcon_(brush)}"
            name="${brush}" label="${this.getLabel_(brush)}">
        </selectable-icon-button>`)}
    </cr-radio-group>
  <!--_html_template_end_-->`;
  // clang-format on
}
