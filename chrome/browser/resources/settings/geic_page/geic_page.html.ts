// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsGeicPageElement} from './geic_page.js';

export function getHtml(this: SettingsGeicPageElement) {
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{geicSectionTitle}">
  <div>
    <cr-link-row id="geicLinkRow"
        start-icon="settings20:button-magic"
        label="$i18n{geicSectionTitle}"
        @click="${this.onGeicPageClick_}">
      <div slot="sub-label">
        $i18n{glicRowSublabel}
      </div>
    </cr-link-row>
  </div>
</settings-section>
<!--_html_template_end_-->`;
}
