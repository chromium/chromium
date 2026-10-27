// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsAntiAbusePageElement} from './anti_abuse_page.js';

export function getHtml(this: SettingsAntiAbusePageElement) {
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{siteSettingsAntiAbuse}"
    route-path="${this.routePath}">
  <settings-toggle-button
      id="toggleButton"
      pref-key="synthetic.settings.site_settings.anti_abuse"
      no-set-pref
      label="$i18n{siteSettingsAntiAbuse}"
      sub-label="$i18n{siteSettingsAntiAbuseDescription}"
      @settings-boolean-control-change="${this.onSettingsBooleanControlChange_}">
  </settings-toggle-button>
  <div class="settings-columned-section">
    <div class="column">
      <h2 class="description-header">$i18n{columnHeadingWhenOn}</h2>
      <ul class="icon-bulleted-list">
        <li>
          <cr-icon icon="settings20:archive" aria-hidden="true"></cr-icon>
          <div class="secondary">$i18n{antiAbuseWhenOnSectionOne}</div>
        </li>
        <li>
          <cr-icon icon="settings20:dashboard" aria-hidden="true"></cr-icon>
          <div class="secondary">$i18n{antiAbuseWhenOnSectionTwo}</div>
        </li>
        <li>
          <cr-icon icon="settings20:timer" aria-hidden="true"></cr-icon>
          <div class="secondary">$i18n{antiAbuseWhenOnSectionThree}</div>
        </li>
      </ul>
    </div>
    <div class="column">
      <h2 class="description-header">$i18n{columnHeadingConsider}</h2>
      <ul class="icon-bulleted-list">
        <li>
          <cr-icon icon="settings20:background-replace" aria-hidden="true">
          </cr-icon>
          <div class="secondary">$i18n{antiAbuseThingsToConsiderSectionOne}</div>
        </li>
      </ul>
    </div>
  </div>
</settings-subpage>
<!--_html_template_end_-->`;
}
