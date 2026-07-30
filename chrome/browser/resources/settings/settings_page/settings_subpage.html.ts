// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsSubpageElement} from './settings_subpage.js';

export function getHtml(this: SettingsSubpageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="cr-row first" id="headerLine">
  <cr-icon-button class="icon-arrow-back" id="closeButton"
      @click="${this.onBackClick_}"
      aria-label="${this.getBackButtonAriaLabel_()}"
      aria-roledescription="${this.getBackButtonAriaRoleDescription_()}">
  </cr-icon-button>
  ${this.titleIcon ? html`
    <img id="title-icon" src="${this.titleIcon}" aria-hidden="true">
  ` : ''}
  ${this.faviconSiteUrl ? html`
    <site-favicon id="favicon" url="${this.faviconSiteUrl}" aria-hidden="true">
    </site-favicon>
  ` : ''}
  <h1 class="cr-title-text">${this.pageTitle}</h1>
  <slot name="subpage-title-extra"></slot>
  ${this.learnMoreUrl ? html`
    <cr-icon-button iron-icon="cr:help" suppress-rtl-flip
        aria-label="${this.getLearnMoreAriaLabel_()}"
        aria-description="$i18n{opensInNewTab}"
        @click="${this.onHelpClick_}">
    </cr-icon-button>
  ` : ''}
  ${this.searchLabel ? html`
    <cr-search-field label="${this.searchLabel}"
        icon-override="${this.searchIcon}"
        @search-changed="${this.onSearchChanged_}"
        clear-label="$i18n{clearSearch}">
    </cr-search-field>
  ` : ''}
</div>
<slot></slot>
<!--_html_template_end_-->`;
  // clang-format on
}
