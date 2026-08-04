// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsSearchEngineIconElement} from './search_engine_icon.js';

export function getHtml(this: SettingsSearchEngineIconElement) {
  return html`<!--_html_template_start_-->
<img id="downloadedIcon" is="cr-auto-img"
    auto-src="${this.getIconUrl_()}" clear-src
    @load="${this.onDownloadedIconLoad_}"
    @error="${this.onDownloadedIconError_}" alt=""
    ?hidden="${!this.shouldShowDownloadedIcon_()}">
<site-favicon .faviconUrl="${this.engine?.iconURL || ''}"
    .url="${this.engine?.url || ''}"
    .iconPath="${this.engine?.iconPath || ''}" aria-hidden="true"
    ?hidden="${this.shouldShowDownloadedIcon_()}">
</site-favicon>
<!--_html_template_end_-->`;
}
