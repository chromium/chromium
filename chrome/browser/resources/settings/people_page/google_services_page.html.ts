// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsGoogleServicesPageElement} from './google_services_page.js';

export function getHtml(this: SettingsGoogleServicesPageElement) {
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{googleServicesPageTitle}"
    route-path="${this.routePath}">
  ${this.shouldShowPageContents_() ? html`
    <settings-personalization-options .syncStatus="${this.syncStatus_}">
    </settings-personalization-options>
  ` : ''}
</settings-subpage>
<!--_html_template_end_-->`;
}
