// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsRadioGroupElement} from './settings_radio_group.js';

export function getHtml(this: SettingsRadioGroupElement) {
  return html`<!--_html_template_start_-->
<cr-radio-group .selected="${this.selected}"
    @selected-changed="${this.onSelectedChanged_}"
    aria-label="${this.groupAriaLabel}"
    .selectableElements="${this.selectableElements}"
    ?nested-selectable="${this.nestedSelectable}">
  <slot></slot>
</cr-radio-group>
<!--_html_template_end_-->`;
}
