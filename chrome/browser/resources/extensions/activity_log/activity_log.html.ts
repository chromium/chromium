// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsActivityLogElement} from './activity_log.js';

export function getHtml(this: ExtensionsActivityLogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="page-container" id="container">
  <div class="page-content">
    <div class="page-header">
      <cr-icon-button class="icon-arrow-back no-overlap" id="closeButton"
          aria-label="$i18n{back}" @click="${this.onCloseButtonClick_}">
      </cr-icon-button>
      ${!this.isPlaceholder_() ? html`
        <img id="icon" src="${this.getExtensionIconUrl_()}" alt="">` : ''}
      <div class="cr-title-text">
        ${this.getActivityLogHeading_()}
      </div>
    </div>
    <cr-tabs id="tabs" selected="${this.selectedSubpage_}"
        @selected-changed="${this.onTabsChangedSelectedSubpage_}"
        .tabNames="${this.tabNames_}">
    </cr-tabs>
    <cr-page-selector selected="${this.selectedSubpage_}">
      <div>
        ${this.isHistoryTabSelected_() ? html`
          <activity-log-history extension-id="${this.extensionInfo.id}"
              .delegate="${this.delegate}">
          </activity-log-history>` : ''}
      </div>
      <div>
        <activity-log-stream extension-id="${this.extensionInfo.id}"
            .delegate="${this.delegate}"
            ?hidden="${!this.isStreamTabSelected_()}">
        </activity-log-stream>
      </div>
    </cr-page-selector>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
