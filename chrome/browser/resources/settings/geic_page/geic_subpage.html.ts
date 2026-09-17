// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsGeicSubpageElement} from './geic_subpage.js';

export function getHtml(this: SettingsGeicSubpageElement) {
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{geicSectionTitle}"
    route-path="${this.routePath}">
  <div class="section">
    <h2 class="cr-title-text">$i18n{glicPreferencesSection}</h2>
    <settings-toggle-button
        id="tabstripButtonToggle"
        pref-key="geic.pinned_to_tabstrip"
        label="$i18n{glicTabstripButtonToggle}"
        sub-label="$i18n{glicTabstripButtonToggleSublabel}">
    </settings-toggle-button>
  </div>
</settings-subpage>
<!--_html_template_end_-->`;
}
