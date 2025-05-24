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
<div id="centerContainer">
  ${this.managementNotice_ ?
      html`<div id="managementNoticeContainer">
        <img id="managementNoticeLogo" alt=""
            src="${this.managementNotice_.bitmapDataUrl.url}">
        <p title="${this.managementNotice_.text}">
          ${this.managementNotice_.text}
        </p>
      </div>` : ''}
  ${this.extensionName_ ?
      html`<div id="extensionName" title="${this.extensionName_}">
        <button @click="${this.onExtensionNameClick_}" role="link"
            aria-roledescription="$i18n{currentTabLinkRoleDesc}"
            aria-label="$i18n{currentTabLinkLabel}">
            ${this.extensionName_}
        </button>
      </div>` : ''}
</div>
<!--_html_template_end_-->`;
}