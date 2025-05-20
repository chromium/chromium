// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './icons.html.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TextStylesSelectorElement} from './text_styles_selector.js';

export function getHtml(this: TextStylesSelectorElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    ${this.getTextStyles_().map(style => html`
      <cr-icon-button class="${this.getActiveClass_(style)}"
          @click="${this.onStyleButtonClick_}"
          data-style="${style}"
          iron-icon="pdf-ink:text-format-${style}"
          aria-pressed="${this.getAriaPressed_(style)}"
          aria-label="${this.getTitle_(style)}"
          title="${this.getTitle_(style)}">
      </cr-icon-button>`)}
  <!--_html_template_end_-->`;
  // clang-format on
}
