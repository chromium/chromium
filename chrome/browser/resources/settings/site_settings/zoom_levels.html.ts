// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ZoomLevelsElement} from './zoom_levels.js';

export function getHtml(this: ZoomLevelsElement) {
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{siteSettingsCategoryZoomLevels}"
    route-path="${this.routePath}">
  <div class="content-settings-header secondary">
    $i18n{siteSettingsZoomLevelsDescription}
  </div>
  <div class="list-frame vertical-list">
    <div id="list" role="listbox" focusgroup="listbox block"
        class="cr-separators">
      ${this.sites_.map((item, index) => html`
        <div class="list-item" ?first="${!index}">
          <site-favicon .url="${item.originForFavicon}"></site-favicon>
          <div class="middle">
            <span class="url-directionality">${item.displayName}</span>
          </div>
          <div class="zoom-label">${item.zoom}</div>
          <cr-icon-button class="icon-clear" data-index="${index}"
              @click="${this.onRemoveZoomLevelClick_}"
              title="$i18n{siteSettingsRemoveZoomLevel}">
          </cr-icon-button>
        </div>
      `)}
    </div>
    <div id="empty" ?hidden="${!this.shouldShowNoSites_()}">
      $i18n{siteSettingsNoZoomedSites}
    </div>
  </div>
</settings-subpage>
<!--_html_template_end_-->`;
}
