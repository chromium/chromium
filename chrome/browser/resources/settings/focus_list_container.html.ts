// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsFocusListContainerElement} from './focus_list_container.js';

export function getHtml(this: SettingsFocusListContainerElement) {
  return html`<!--_html_template_start_-->
<slot @slotchange="${this.onSlotchange_}"></slot>
<!--_html_template_end_-->`;
}
