// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsHomePageIndexElement} from './home_page_index.js';

export function getHtml(this: SettingsHomePageIndexElement) {
  return html`<!--_html_template_start_-->
<cr-view-manager id="viewManager" class="cr-centered-card-container"
    ?show-all="${this.shouldShowAll}">
  <settings-home-page slot="view" id="parent"
      route-path="${this.routes_.BASIC.path}">
  </settings-home-page>
</cr-view-manager>
<!--_html_template_end_-->`;
}
