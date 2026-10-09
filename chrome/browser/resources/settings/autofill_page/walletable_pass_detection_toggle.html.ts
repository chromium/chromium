// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsWalletablePassDetectionToggleElement} from './walletable_pass_detection_toggle.js';

export function getHtml(this: SettingsWalletablePassDetectionToggleElement) {
  return html`<!--_html_template_start_-->
<settings-toggle-button id="toggle"
    @settings-boolean-control-change="${this.onSettingsBooleanControlChange_}"
    .pref="${this.walletablePassDetectionOptedIn_}"
    ?disabled="${this.ineligibleUser_}"
    no-extension-indicator
    label="$i18n{walletablePassDetectionToggleLabel}"
    sub-label="$i18n{walletablePassDetectionToggleSubLabel}">
</settings-toggle-button>

<div class="settings-columned-section">
  <div class="column">
    <h3 class="description-header">$i18n{columnHeadingWhenOn}</h3>
    <ul class="icon-bulleted-list">
      <li>
        <cr-icon icon="settings20:sync-saved-locally" aria-hidden="true">
        </cr-icon>
        <div class="cr-secondary-text">
          $i18n{walletablePassDetectionWhenOnSavedInfo}
        </div>
      </li>
      <li>
        <cr-icon icon="privacy:notifications"
            aria-hidden="true"></cr-icon>
        <div class="cr-secondary-text">
          $i18n{walletablePassDetectionWhenOnNotifications}
        </div>
      </li>
    </ul>
  </div>
  <div class="column">
    <h3 class="description-header">$i18n{columnHeadingConsider}</h3>
    <ul class="icon-bulleted-list">
      <li>
        <cr-icon icon="settings20:google" aria-hidden="true"></cr-icon>
        <div class="cr-secondary-text">
          $i18n{walletablePassDetectionToConsiderDataUsage}
        </div>
      </li>
      <li>
        <cr-icon icon="settings20:wallet" aria-hidden="true"></cr-icon>
        <div class="cr-secondary-text">
          $i18n{walletablePassDetectionToConsiderDataStorage}
        </div>
      </li>
    </ul>
  </div>
</div>
<!--_html_template_end_-->`;
}
