// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SitePermissionsSiteGroupElement} from './site_permissions_site_group.js';

export function getHtml(this: SitePermissionsSiteGroupElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="etld-row" class="${this.getClassForIndex_()}">
  <div class="site-favicon"
      .style="background-image:${this.getEtldOrSiteFaviconUrl_()}">
  </div>
  <div class="site-and-subtext">
    <div class="site-wrapper">
      <span id="etldOrSite" class="site">${this.getDisplayUrl_()}</span>
      <span id="etldOrSiteIncludesSubdomains" class="includes-subdomains"
          ?hidden="${!this.etldOrFirstSiteMatchesSubdomains_()}">
        $i18n{sitePermissionsIncludesSubdomains}
      </span>
    </div>
    <span id="etldOrSiteSubtext" class="site-subtext">
      ${this.getEtldOrSiteSubText_()}
    </span>
  </div>
  ${this.isExpandable_() ? html`
    <cr-expand-button no-hover id="expand-sites-button"
        ?expanded="${this.expanded_}"
        @expanded-changed="${this.onExpandedChanged_}">
    </cr-expand-button>` : html`
    <cr-icon-button class="subpage-arrow" id="edit-one-site-button"
        @click="${this.onEditSiteClick_}">
    </cr-icon-button>`}
</div>
<div id="sites-list" ?hidden="${!this.expanded_}">
  ${this.data.sites.map((item, index) => html`
    <div class="site-row hr">
      <div class="site-favicon"
          .style="background-image:${this.getFaviconUrl_(item.site)}">
      </div>
      <div class="site-and-subtext">
        <div class="site-wrapper">
          <span class="site">
            ${this.getSiteWithoutSubdomainSpecifier_(item.site)}
          </span>
          <span class="includes-subdomains"
              ?hidden="${!this.matchesSubdomains_(item.site)}">
            $i18n{sitePermissionsIncludesSubdomains}
          </span>
        </div>
        <span class="site-subtext">${this.getSiteSubtext_(item)}</span>
      </div>
      <cr-icon-button class="subpage-arrow" data-index="${index}"
          @click="${this.onEditSiteInListClick_}">
      </cr-icon-button>
    </div>`)}
</div>

${this.showEditSitePermissionsDialog_ ? html`
  <site-permissions-edit-permissions-dialog .delegate="${this.delegate}"
      .extensions="${this.extensions}" .site="${this.siteToEdit_!.site}"
      .originalSiteSet="${this.siteToEdit_!.siteSet}"
      @close="${this.onEditSitePermissionsDialogClose_}">
  </site-permissions-edit-permissions-dialog>` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
