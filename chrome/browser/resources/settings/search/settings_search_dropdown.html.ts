// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsSearchDropdownElement} from './settings_search_dropdown.js';

export function getHtml(this: SettingsSearchDropdownElement) {
  return html`<!--_html_template_start_-->
${this.results_.map(item => html`
  <div class="result">${item.title}</div>
`)}
<!--_html_template_end_-->`;
}
