// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './selectable_icon_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {InkSizeSelectorElement} from './ink_size_selector.js';

export function getHtml(this: InkSizeSelectorElement) {
  return html`<!--_html_template_start_-->
    <cr-radio-group selectable-elements="selectable-icon-button"
        .selected="${this.currentSizeString_()}" aria-label="$i18n{ink2Size}"
        @selected-changed="${this.onSelectedChanged_}">
      ${this.getCurrentBrushSizes_().map(item => html`
        <selectable-icon-button icon="pdf-ink:${item.icon}"
            name="${item.size}" label="${this.i18n(item.label)}">
        </selectable-icon-button>
      `)}
    </cr-radio-group>
  <!--_html_template_end_-->`;
}
