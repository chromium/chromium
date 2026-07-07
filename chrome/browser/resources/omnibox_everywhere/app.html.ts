// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxEverywhereAppElement} from './app.js';

export function getHtml(this: OmniboxEverywhereAppElement) {
  return html`<!--_html_template_start_-->
<div id="content">
  ${
      this.isComposeboxMode_ ? html`
    <omnibox-everywhere-composebox id="composebox" searchbox-next-enabled
        searchbox-layout-mode="${this.searchboxLayoutMode_}"
        .state="${this.composeboxState_}"
        @close-composebox="${this.onCloseComposebox_}"
        @composebox-submit="${this.onComposeboxSubmit_}"
        .showVoiceSearch="${true}"
        .usePecApi="${this.usePecApi_}"
        .isOblongShape="${this.isOblongShape_}"
        .contextManagementInComposeboxEnabled="${
                                   this.contextManagementInComposeboxEnabled_}"
        entrypoint-name="Omnibox">
    </omnibox-everywhere-composebox>
  ` :
                               html`
    <omnibox-everywhere-omnibox id="searchbox"
        @open-composebox="${this.onOpenComposebox_}"
        .contextManagementInComposeboxEnabled="${
                                   this.contextManagementInComposeboxEnabled_}">
    </omnibox-everywhere-omnibox>
  `}
</div>
<!--_html_template_end_-->`;
}
