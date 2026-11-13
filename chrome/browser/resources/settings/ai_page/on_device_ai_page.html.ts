// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsOnDeviceAiPageElement} from './on_device_ai_page.js';

export function getHtml(this: SettingsOnDeviceAiPageElement) {
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{onDeviceAiEnabledLabel}"
    route-path="${this.routePath}">
  <settings-toggle-button id="onDeviceAiToggle"
      pref-key="synthetic.settings.on_device_ai_enabled"
      label="$i18n{onDeviceAiCapabilitiesLabel}"
      sub-label-with-link="$i18n{onDeviceAiCapabilitiesSubLabel}"
      @sub-label-link-clicked="${this.onOnDeviceAiSubLabelLinkClicked_}"
      @settings-boolean-control-change="${
          this.onOnDeviceAiSettingsBooleanControlChange_}">
  </settings-toggle-button>
  <div class="settings-columned-section">
    <div class="column">
      <h2 class="description-header">$i18n{columnHeadingWhenOn}</h2>
      <ul class="icon-bulleted-list">
        <li>
          <cr-icon icon="settings20:chrome-product" aria-hidden="true">
          </cr-icon>
          <div class="secondary">$i18n{onDeviceAiWhenOnFeatures}</div>
        </li>
        <li>
          <cr-icon icon="settings20:history" aria-hidden="true"></cr-icon>
          <div class="secondary">$i18n{onDeviceAiWhenOnAutoUpdates}</div>
        </li>
      </ul>
    </div>
    <div class="column">
      <h2 class="description-header">$i18n{columnHeadingConsider}</h2>
      <ul class="icon-bulleted-list">
        <li>
          <cr-icon icon="settings20:block" aria-hidden="true"></cr-icon>
          <div class="secondary">$i18n{onDeviceAiConsiderWhenOff}</div>
        </li>
        <li>
          <cr-icon icon="settings20:download" aria-hidden="true"></cr-icon>
          <div class="secondary">$i18n{onDeviceAiConsiderStorage}</div>
        </li>
        <li>
          <cr-icon icon="settings20:globe" aria-hidden="true"></cr-icon>
          <div class="secondary">$i18n{onDeviceAiConsiderSitePrivacy}</div>
        </li>
      </ul>
    </div>
  </div>
</settings-subpage>
<!--_html_template_end_-->`;
}
