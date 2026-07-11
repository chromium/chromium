// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsAppearancePageIndexElement} from './appearance_page_index.js';

export function getHtml(this: SettingsAppearancePageIndexElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-view-manager id="viewManager" class="cr-centered-card-container"
    ?show-all="${this.shouldShowAll}">
  <settings-appearance-page slot="view" id="parent"
      route-path="${this.routes_.APPEARANCE.path}">
  </settings-appearance-page>

  <settings-appearance-fonts-page slot="view" id="fonts"
      data-parent-view-id="parent" route-path="${this.routes_.FONTS.path}">
  </settings-appearance-fonts-page>
</cr-view-manager>
<!--_html_template_end_-->`;
  // clang-format on
}
