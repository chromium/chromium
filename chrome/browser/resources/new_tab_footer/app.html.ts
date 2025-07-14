// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NewTabFooterAppElement} from './app.js';

export function getHtml(this: NewTabFooterAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<!--
Container for housing the items in the center of the footer that are
separated from each other by a divider.
-->
<div id="container" @contextmenu="${this.onContextMenu_}">
  <div id="infoContainer">
    ${this.managementNotice_ ? html`
      <div id="managementNoticeContainer" class="notice-item">
        <img id="managementNoticeLogo" alt=""
            src="${this.managementNotice_.bitmapDataUrl.url}">
        <p title="${this.managementNotice_.text}">
          ${this.managementNotice_.text}
        </p>
      </div>` : ''}
    ${this.extensionName_ ? html`
      <div id="extensionNameContainer" title="${this.extensionName_}"
          class="notice-item">
        <button @click="${this.onExtensionNameClick_}" role="link"
            aria-label="${this.extensionName_}"
            aria-description="$i18n{manageExtension}">
          ${this.extensionName_}
        </button>
      </div>` : ''}
  </div>
  <ntp-customize-buttons id="customizeButtons"
      ?info-shown-to-user="${this.managementNotice_ || this.extensionName_}"
      ?show-customize="${this.showCustomize_}"
      ?show-customize-chrome-text="${this.showCustomizeChromeText_}"
      @customize-click="${this.onCustomizeClick_}">
  </ntp-customize-buttons>
</div>
<!--_html_template_end_-->`;
  // clang-format off
}
