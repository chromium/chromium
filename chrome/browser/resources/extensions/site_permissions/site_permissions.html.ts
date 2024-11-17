// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsSitePermissionsElement} from './site_permissions.js';

export function getHtml(this: ExtensionsSitePermissionsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="page-container" id="container">
  <div id="header">$i18n{sitePermissionsPageTitle}</div>
  <div id="site-permissions-container">
    <div id="site-lists">
      ${this.showPermittedSites_ ? html`
        <site-permissions-list .delegate="${this.delegate}"
            .extensions="${this.extensions}" header="$i18n{permittedSites}"
            .siteSet="${chrome.developerPrivate.SiteSet.USER_PERMITTED}"
            .sites="${this.permittedSites}">
        </site-permissions-list>` : ''}
      <site-permissions-list .delegate="${this.delegate}"
          .extensions="${this.extensions}" header="$i18n{restrictedSites}"
          .siteSet="${chrome.developerPrivate.SiteSet.USER_RESTRICTED}"
          .sites="${this.restrictedSites}">
      </site-permissions-list>
    </div>
    <cr-link-row class="hr" id="allSitesLink"
        label="$i18n{sitePermissionsViewAllSites}"
        @click="${this.onAllSitesLinkClick_}">
    </cr-link-row>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
