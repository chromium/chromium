// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsGeneralPageIndexElement} from './general_page_index.js';

export function getHtml(this: SettingsGeneralPageIndexElement) {
  return html`<!--_html_template_start_-->
<cr-view-manager id="viewManager" class="cr-centered-card-container"
    ?show-all="${this.shouldShowAll}">
  <settings-on-startup-page slot="view" id="onStartup">
  </settings-on-startup-page>

  <settings-downloads-page slot="view" id="downloads">
  </settings-downloads-page>

<if expr="not is_chromeos">
  <settings-system-page slot="view" id="system">
  </settings-system-page>
</if>
</cr-view-manager>
<!--_html_template_end_-->`;
}
