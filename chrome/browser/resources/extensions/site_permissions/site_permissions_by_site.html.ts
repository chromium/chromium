// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsSitePermissionsBySiteElement} from './site_permissions_by_site.js';

export function getHtml(this: ExtensionsSitePermissionsBySiteElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="page-container" id="container">
  <div class="page-content">
    <div class="page-header">
      <cr-icon-button class="icon-arrow-back no-overlap" id="closeButton"
          @click="${this.onCloseButtonClick_}">
      </cr-icon-button>
      <span class="cr-title-text">$i18n{sitePermissionsAllSitesPageTitle}</span>
    </div>
    <div id="site-groups">
      ${this.siteGroups_.map((item, index) => html`
        <site-permissions-site-group .data="${item}"
            .delegate="${this.delegate}" .extensions="${this.extensions}"
            list-index="${index}">
        </site-permissions-site-group>`)}
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
