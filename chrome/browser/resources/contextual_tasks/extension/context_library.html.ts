// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ContextLibraryElement} from './context_library.js';

export function getHtml(this: ContextLibraryElement) {
  // Frontend context library is synonymous with favicon group.
  return html`<!--_html_template_start_-->
    <composebox-favicon-group id="faviconGroup"
        .tabs="${this.tabs}"
        .submittedTabIds="${this.submittedTabIds}"
        @wait-for-tab-load="${this.onWaitForTabLoad_}">
    </composebox-favicon-group>
  <!--_html_template_end_-->`;
}
