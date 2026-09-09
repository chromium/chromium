// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsClearBrowsingDataAccountIndicatorElement} from './clear_browsing_data_account_indicator.js';

export function getHtml(
    this: SettingsClearBrowsingDataAccountIndicatorElement) {
  return html`<!--_html_template_start_-->
${this.shouldShowAccountIndicator_ ? html`
  <div id="avatarRow" class="cr-row first two-line">
    <img class="account-icon" alt=""
        src="${this.getProfileImageSrc_()}">
    <div id="userInfo" class="cr-row-gap cr-padded-text flex no-min-width">
      <div class="text-elide">${this.shownAccount_?.fullName}</div>
      <div class="text-elide secondary">${this.shownAccount_?.email}</div>
    </div>
  </div>
` : ''}
<!--_html_template_end_-->`;
}
