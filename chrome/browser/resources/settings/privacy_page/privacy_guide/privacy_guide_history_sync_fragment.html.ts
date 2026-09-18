// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PrivacyGuideHistorySyncFragmentElement} from './privacy_guide_history_sync_fragment.js';

export function getHtml(this: PrivacyGuideHistorySyncFragmentElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="settings-fragment-header" focus-element tabindex="-1">
  <picture>
    <source
        srcset="./images/privacy_guide/history_sync_graphic_dark_v2.svg"
        media="(prefers-color-scheme: dark)">
    <img alt="" src="./images/privacy_guide/history_sync_graphic_v2.svg">
  </picture>
  <h2 class="settings-fragment-header-label">
    ${this.getHistorySyncCardHeader_()}
  </h2>
</div>
<div class="fragment-content">
  <div class="embedded-setting-wrapper">
    <settings-toggle-button id="historyToggle"
        .pref="${this.historySyncVirtualPref_}"
        @change="${this.onToggleChange_}"
        label="${this.getHistorySyncToggleLabel_()}">
    </settings-toggle-button>
  </div>
  <div class="settings-columned-section">
    <div class="column">
      <h3 class="description-header">
        $i18n{columnHeadingWhenOn}
      </h3>
      <ul class="icon-bulleted-list">
        <li>
          <cr-icon icon="settings20:history" aria-hidden="true"></cr-icon>
          <div class="secondary">
            ${this.getHistorySyncFeatureDescription1_()}
          </div>
        </li>
        <li>
          <cr-icon icon="settings20:dns" aria-hidden="true"></cr-icon>
          <div class="secondary">
            $i18n{privacyGuideHistorySyncFeatureDescription2}
          </div>
        </li>
      </ul>
    </div>
    <div class="column">
      <h3 class="description-header">$i18n{columnHeadingConsider}</h3>
      <ul class="icon-bulleted-list">
        <li>
          <cr-icon icon="settings20:link" aria-hidden="true"></cr-icon>
          <div class="secondary">
            $i18n{privacyGuideHistorySyncPrivacyDescription1}
          </div>
        </li>
      </ul>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
