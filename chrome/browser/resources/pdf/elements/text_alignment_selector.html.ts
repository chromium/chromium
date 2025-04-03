// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import './icons.html.js';
import './selectable_icon_button.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {TextAlignment} from '../constants.js';

import type {TextAlignmentSelectorElement} from './text_alignment_selector.js';

export function getHtml(this: TextAlignmentSelectorElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <cr-radio-group selectable-elements="selectable-icon-button"
        .selected="${this.currentAlignment_}"
        @selected-changed="${this.onSelectedAlignmentChanged_}">
      <selectable-icon-button icon="pdf:text-align-left"
          name="${TextAlignment.LEFT}" label="Left">
      </selectable-icon-button>
      <selectable-icon-button icon="pdf:text-align-center"
          name="${TextAlignment.CENTER}" label="Center">
      </selectable-icon-button>
      <selectable-icon-button icon="pdf:text-align-justify"
          name="${TextAlignment.JUSTIFY}" label="Justify">
      </selectable-icon-button>
      <selectable-icon-button icon="pdf:text-align-right"
          name="${TextAlignment.RIGHT}" label="Right">
      </selectable-icon-button>
    </cr-radio-group>
  <!--_html_template_end_-->`;
  // clang-format on
}
