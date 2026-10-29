// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {SettingsTabHoverPreviewPageElement} from './tab_hover_preview_page.js';

export function getHtml(this: SettingsTabHoverPreviewPageElement) {
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{hoverCardTitle}">
  ${this.showHoverCardImagesOption_ ? html`
    <settings-toggle-button id="hoverCardImagesToggle"
        @settings-boolean-control-change="${
            this.onHoverCardImagesSettingsBooleanControlChange_}"
        pref-key="browser.hovercard.image_previews_enabled"
        label="$i18n{showHoverCardImages}">
    </settings-toggle-button>
  ` : ''}
  <settings-toggle-button id="hoverCardMemoryUsageToggle"
      @settings-boolean-control-change="${
          this.onHoverCardMemoryUsageSettingsBooleanControlChange_}"
      pref-key="browser.hovercard.memory_usage_enabled"
      label="$i18n{showHoverCardMemoryUsage}">
  </settings-toggle-button>
</settings-section>
<!--_html_template_end_-->`;
}
