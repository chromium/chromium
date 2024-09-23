// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BuyingOptionsSectionElement} from './buying_options_section.js';

export function getHtml(this: BuyingOptionsSectionElement) {
  return html`<!--_html_template_start_-->
  <div id="link" @click="${this.openJackpotUrl_}">
    <span class="link-text">$i18n{buyingOptions}</span>
    <cr-icon icon="cr:open-in-new"></cr-icon>
  </div>
  <!--_html_template_end_-->`;
}
