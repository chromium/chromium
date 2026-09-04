// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsLanguagesPageIndexCrosElement} from './languages_page_index_cros.js';

export function getHtml(this: SettingsLanguagesPageIndexCrosElement) {
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{languagesPageTitle}"
    class="cr-centered-card-container">
  <cr-link-row id="openChromeOSLanguagesSettings"
      @click="${this.onOpenChromeOsLanguagesSettingsClick_}"
      label="$i18n{openChromeOSLanguagesSettingsLabel}" external>
  </cr-link-row>
</settings-section>
<!--_html_template_end_-->`;
}
