// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ReopenTabsElement} from './reopen_tabs.js';

export function getHtml(this: ReopenTabsElement) {
  return html`<!--_html_template_start_-->
    <div id="reopenContainer">
      <div id="reopenText">$i18n{continueThread}</div>
      <div class="reopen-buttons">
        <cr-button class="action" aria-label="$i18n{reopenTab}"
            @click="${this.onReopenClick_}">
          $i18n{reopenTab}
        </cr-button>
        <cr-icon-button id="reopenDismiss" iron-icon="cr:close"
            @click="${this.onDismissClick_}">
        </cr-icon-button>
      </div>
    </div>
  <!--_html_template_end_-->`;
}
