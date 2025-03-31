// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import './icons.html.js';
import './selectable_icon_button.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {TextAlignment} from '../constants.js';

import type {ViewerTextSidePanelElement} from './viewer_text_side_panel.js';

export function getHtml(this: ViewerTextSidePanelElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <div class="side-panel-content">
      <h2>Font</h2>
      <select class="md-select" style="font-family: '${this.currentFont_}';"
          @change="${this.onFontChange_}"=>
        ${this.fonts_.map(font => html`
          <option value="${font}" ?selected="${this.isSelectedFont_(font)}">
            ${font}
          </option>`)}
      </select>
      <select class="md-select" @change="${this.onSizeChange_}">
        ${this.sizes_.map(size => html`
          <option value="${size}" ?selected="${this.isSelectedSize_(size)}">
            ${size}
          </option>`)}
      </select>
    </div>
    <div class="side-panel-content">
      <h2>Styles</h2>
      <div class="style-buttons">
        ${this.getTextStyles_().map(style => html`
          <cr-icon-button class="${this.getActiveClass_(style)}"
              @click="${this.onStyleButtonClick_}"
              data-style="${style}"
              iron-icon="pdf:text-format-${style}"
              aria-pressed="${this.getAriaPressed_(style)}"
              aria-label="${style}"
              title="${style}">
          </cr-icon-button>`)}
      </div>
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
    </div>
  <!--_html_template_end_-->`;
  // clang-format on
}
