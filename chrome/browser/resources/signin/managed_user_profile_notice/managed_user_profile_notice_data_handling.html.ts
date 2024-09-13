// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowsingDataHandling} from './managed_user_profile_notice_browser_proxy.js';
import type {ManagedUserProfileNoticeDataHandlingElement} from './managed_user_profile_notice_data_handling.js';

export function getHtml(this: ManagedUserProfileNoticeDataHandlingElement) {
  return html`<!--_html_template_start_-->
<main class="tangible-sync-style">
  <img class="success-icon" alt="">
  <div id="text-container">
    <h1 class="title">
      ${this.title}
    </h1>
  </div>
  <cr-radio-group .selected="${this.selectedDataHandling}"
      @selected-changed="${this.onSelectedRadioOptionChanged_}">
    <cr-radio-button name="${BrowsingDataHandling.SEPARATE}"
        label="$i18n{separateBrowsingDataChoiceTitle}">
      <p>$i18n{separateBrowsingDataChoiceDetails}</p>
    </cr-radio-button>
    <cr-radio-button name="${BrowsingDataHandling.MERGE}"
        label="$i18n{mergeBrowsingDataChoiceTitle}">
      <p>$i18n{mergeBrowsingDataChoiceDetails}</p>
  </cr-radio-button>
</main>
<!--_html_template_end_-->`;
}
