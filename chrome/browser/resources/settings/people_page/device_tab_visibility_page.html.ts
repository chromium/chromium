// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsDeviceTabVisibilityPageElement} from './device_tab_visibility_page.js';

export function getHtml(this: SettingsDeviceTabVisibilityPageElement) {
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{deviceTabVisibilityTitle}"
    route-path="${this.routePath}">
  <div id="description" class="cr-secondary-text">
    $i18nRaw{deviceTabVisibilityDescription}
  </div>
  <div id="noDevicesLabel" class="list-frame cr-secondary-text"
      ?hidden="${this.devices_.length > 0}">
    $i18n{deviceTabVisibilityNoDevices}
  </div>
  ${this.devices_.length > 0 ? html`
    <div id="deviceList" class="list-frame">
      ${this.devices_.map((device, index) => html`
        <div class="list-item device-row">
          <cr-icon icon="${this.getDeviceIcon_(device.formFactor)}"></cr-icon>
          <div class="device-content">
            <div class="cr-padded-text">
              <div class="device-name">${device.deviceName}</div>
              <div class="cr-secondary-text" id="deviceTime_${index}">
                ${device.lastActiveTime}
              </div>
            </div>
            <cr-toggle .checked="${device.visible}"
                aria-label="${device.deviceName}"
                aria-describedby="deviceTime_${index}"
                data-session-tag="${device.sessionTag}"
                @change="${this.onDeviceVisibilityToggleChange_}">
            </cr-toggle>
          </div>
        </div>
      `)}
    </div>
  ` : ''}
</settings-subpage>
<!--_html_template_end_-->`;
}
