// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ContextLibraryElement} from './context_library.js';

export function getHtml(this: ContextLibraryElement) {
  // Frontend context library renders either the Smart Tab Sharing active coin
  // or the manual/restored tab favicon group.
  return html`<!--_html_template_start_-->
    ${this.smartTabSharingActive ? html`
      <cr-icon id="stsCoin" class="sts-active-coin"
          icon="composebox:screensaverAuto">
      </cr-icon>
    ` : ''}
    <composebox-favicon-group id="faviconGroup"
        ?hidden="${this.smartTabSharingActive}"
        .tabs="${this.tabs}"
        .submittedTabIds="${this.submittedTabIds}"
        @wait-for-tab-load="${this.onWaitForTabLoad_}">
    </composebox-favicon-group>
  <!--_html_template_end_-->`;
}
