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
      sub-label-with-link="$i18n{onDeviceAiEnabledSubLabel}"
      @sub-label-link-clicked="${this.onOnDeviceAiSubLabelLinkClicked_}"
      @settings-boolean-control-change="${
          this.onOnDeviceAiSettingsBooleanControlChange_}">
  </settings-toggle-button>
</settings-subpage>
<!--_html_template_end_-->`;
}
