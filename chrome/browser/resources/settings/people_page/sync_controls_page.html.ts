// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsSyncControlsPageElement} from './sync_controls_page.js';

export function getHtml(this: SettingsSyncControlsPageElement) {
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{syncAdvancedPageTitle}"
    learn-more-url="$i18n{syncAndGoogleServicesLearnMoreURL}"
    route-path="${this.routePath}">
  <settings-sync-controls .syncStatus="${this.syncStatus_}">
  </settings-sync-controls>
</settings-subpage>
<!--_html_template_end_-->`;
}
