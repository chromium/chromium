// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NewTabButtonElement} from './new_tab_button.js';

export function getHtml(this: NewTabButtonElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button id="button"
    iron-icon="cr:add"
    aria-label="$i18n{newTabButtonAccName}"
    title="$i18n{newTabButtonAccName}"
    @click="${this.onClick}">
</cr-icon-button>
<!--_html_template_end_-->`;
}
