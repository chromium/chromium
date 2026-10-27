// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ContentSetting} from './constants.js';
import type {CategorySettingExceptionsElement} from './category_setting_exceptions.js';

export function getHtml(this: CategorySettingExceptionsElement) {
  return html`<!--_html_template_start_-->
    <div id="exceptionHeader">
      <h2>$i18n{siteSettingsCustomizedBehaviors}</h2>
      <div id="exceptionHeaderSubLabel" class="secondary">
        ${this.description}
      </div>
    </div>
    ${this.category ? html`
      <site-list
          .category="${this.category}"
          category-subtype="${ContentSetting.BLOCK}"
          category-header="${this.blockHeader}"
          ?read-only-list="${this.getReadOnlyList_()}"
          search-filter="${this.searchFilter}">
      </site-list>
      <site-list
          .category="${this.category}"
          category-subtype="${ContentSetting.SESSION_ONLY}"
          category-header="$i18n{siteSettingsSessionOnly}"
          ?read-only-list="${this.getReadOnlyList_()}"
          search-filter="${this.searchFilter}">
      </site-list>
      <site-list
          .category="${this.category}"
          category-subtype="${ContentSetting.ALLOW}"
          category-header="${this.allowHeader}"
          ?read-only-list="${this.getReadOnlyList_()}"
          search-filter="${this.searchFilter}"
          ?hidden="${!this.shouldShowAllowSiteList_()}">
      </site-list>
    ` : ''}
<!--_html_template_end_-->`;
}
