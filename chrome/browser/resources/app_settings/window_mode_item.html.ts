// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WindowModeItemElement} from './window_mode_item.js';

export function getHtml(this: WindowModeItemElement) {
  return html`<!--_html_template_start_-->
<app-management-toggle-row
    id="toggle-row"
    label="${this.windowModeLabel}"
    ?value="${this.getValue_()}">
</app-management-toggle-row>
<!--_html_template_end_-->`;
}
