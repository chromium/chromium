// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxEverywhereAppElement} from './everywhere_app.js';

export function getHtml(this: OmniboxEverywhereAppElement) {
  return html`<!--_html_template_start_-->
<div id="content">
  ${
      this.isComposeboxMode_ ? html`
    <everywhere-composebox id="composebox" searchbox-next-enabled
        searchbox-layout-mode="${this.searchboxLayoutMode_}"
        .state="${this.composeboxState_}"
        @close-composebox="${this.onCloseComposebox_}"
        @composebox-submit="${this.onComposeboxSubmit_}"
        .showVoiceSearch="${true}"
        .usePecApi="${this.usePecApi_}"
        .isOblongShape="${this.isOblongShape_}"
        entrypoint-name="Omnibox">
    </everywhere-composebox>
  ` :
                               html`
    <everywhere-searchbox id="searchbox"
        @open-composebox="${this.onOpenComposebox_}">
    </everywhere-searchbox>
  `}
</div>
<!--_html_template_end_-->`;
}
