// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AutoTabGroupsFailureElement} from './auto_tab_groups_failure.js';

export function getHtml(this: AutoTabGroupsFailureElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="auto-tab-groups-container">
  <div class="auto-tab-groups-text-container">
    <div class="auto-tab-groups-body">
      <localized-link localized-string="${this.getBody_()}"
          @link-clicked="${this.onCheckNow_}"></localized-link>
    </div>
  </div>
  ${this.showFre ? html`
    <div class="footer">
      <div class="auto-tab-groups-body">
        <b>$i18n{tipTitle}</b> $i18n{tipBody}
        <div class="auto-tab-groups-link"
            role="link"
            tabindex="0"
            @click="${this.onTipClick_}"
            @keydown="${this.onTipKeyDown_}"
            aria-description="$i18n{tipAriaDescription}">
          $i18n{tipAction}
        </div>
      </div>
    </div>
  ` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
