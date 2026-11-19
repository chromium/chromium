// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsHomePageElement} from './home_page.js';

export function getHtml(this: SettingsHomePageElement) {
  return html`<!--_html_template_start_-->
<settings-section></settings-section>
<!--_html_template_end_-->`;
}
