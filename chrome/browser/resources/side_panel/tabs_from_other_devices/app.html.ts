// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabsFromOtherDevicesAppElement} from './app.js';

export function getHtml(this: TabsFromOtherDevicesAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container">
  ${this.loading_ ? html`<div class="loading">$i18n{loading}</div>` : ''}

  ${!this.loading_ && this.syncedDevices_.length === 0 ?
      html`<div class="empty-message">$i18n{noSyncedResults}</div>` : ''}

  ${!this.loading_ && this.syncedDevices_.length > 0 ?
      html`
    <div id="picker-container">
      <cr-button id="picker-button" @click="${this.onDeviceSelectClick_}">
        ${this.getSelectedDeviceName_()}
        <cr-icon icon="cr:arrow-drop-down"></cr-icon>
      </cr-button>
      <cr-action-menu id="deviceMenu">
        ${this.syncedDevices_.map(device => html`
          <button class="dropdown-item" @click="${this.onDeviceItemClick_}"
                data-tag="${device.tag}">
            ${device.name}
          </button>
        `)}
      </cr-action-menu>
    </div>
  ` : ''}

  <div id="devices">
    ${this.syncedDevices_
        .filter(device => device.tag === this.selectedDeviceTag_)
        .map(device => html`
        <div class="tabs">
          ${device.windows.map(
          window => window.tabs.map(
            tab => html`
            <div class="tab" @click="${this.onTabClick_}"
                  @auxclick="${this.onTabAuxclick_}"
                  data-session-tag="${device.tag}"
                  data-tab-id="${tab.sessionId}">
              <div class="tab-favicon-container">
                <div class="tab-favicon"
                      style="background-image:
                            ${getFaviconForPageURL(tab.url, true)}">
                </div>
              </div>
              <div class="tab-info">
                <span class="tab-title">${tab.title}</span>
                <div class="tab-details">
                  <span class="tab-url">${this.getHostname_(tab.url)}</span>
                  <span class="tab-timestamp-separator">&bull;</span>
                  <span class="tab-timestamp">${tab.timestampDisplayStr}</span>
                </div>
              </div>
            </div>
          `))}
        </div>
    `)}
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
