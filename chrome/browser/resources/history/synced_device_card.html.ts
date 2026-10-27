// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistorySyncedDeviceCardElement} from './synced_device_card.js';

export function getHtml(this: HistorySyncedDeviceCardElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="history-item-container">
  <div class="card-title" id="cardHeading" aria-expanded="${this.opened}"
      aria-controls="collapse" @click="${this.onCardHeadingClick_}">
    <div id="title-left-content">
      <div id="device-name">${this.device}</div>
      <span id="last-update-time">${this.lastUpdateTime}</span>
    </div>
    <div id="right-buttons">
      <cr-icon-button id="menuButton" iron-icon="cr:more-vert"
          @click="${this.onMenuButtonClick_}"
          title="$i18n{actionMenuDescription}">
      </cr-icon-button>
      <cr-icon-button id="collapseButton" iron-icon="${this.getCollapseIcon_()}"
          title="${this.getCollapseTitle_()}">
      </cr-icon-button>
    </div>
  </div>

  <cr-collapse id="collapse" ?opened="${this.opened}"
      @opened-changed="${this.onOpenedChanged_}">
    <div id="tab-item-list">
      ${this.windows.map((win, windowIndex) => html`
        ${this.windows.length > 1 && !this.searchTerm ? html`
          <div class="window-header">
            <div class="window-title">
              <cr-icon icon="history20:tab"></cr-icon>
              <span>${this.getWindowTitle_(windowIndex)}</span>
            </div>
            <cr-icon-button class="open-window-button"
                iron-icon="cr:open-in-new" data-window-id="${win.sessionId}"
                title="${this.getOpenWindowTitle_(windowIndex)}"
                aria-label="${this.getOpenWindowTitle_(windowIndex)}"
                @click="${this.onOpenWindowClick_}">
            </cr-icon-button>
          </div>
        ` : ''}
        ${win.tabs.map(tab => html`
          <div class="item-container">
            <a href="${tab.url}" class="website-link" title="${tab.title}"
                data-session-id="${tab.sessionId}" @click="${this.onLinkClick_}"
                @auxclick="${this.onLinkAuxclick_}"
                @contextmenu="${this.onLinkContextmenu_}">
              <div class="website-icon"></div>
              <history-searched-label class="website-title" title="${tab.title}"
                  search-term="${this.searchTerm}">
              </history-searched-label>
            </a>
          </div>
        `)}
      `)}
    </div>
  </cr-collapse>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
