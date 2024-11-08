// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsHostPermissionsToggleListElement} from './host_permissions_toggle_list.js';

export function getHtml(this: ExtensionsHostPermissionsToggleListElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="section-heading" ?hidden="${this.enableEnhancedSiteControls}">
  <span>$i18n{hostPermissionsDescription}</span>
  <a id="linkIconButton" aria-label="$i18n{permissionsLearnMoreLabel}"
      href="$i18n{hostPermissionsLearnMoreLink}" target="_blank"
      @click="${this.onLearnMoreClick_}">
    <cr-icon icon="cr:help-outline"></cr-icon>
  </a>
</div>
<div class="toggle-section">
  <extensions-toggle-row ?checked="${this.allowedOnAllHosts_()}"
      id="allHostsToggle" @change="${this.onAllHostsToggleChanged_}">
    <span class="${this.getAllHostsToggleLabelClass_()}">
      $i18n{itemAllowOnFollowingSites}
    </span>
    <a id="linkIconButton" aria-label="$i18n{permissionsLearnMoreLabel}"
        href="$i18n{hostPermissionsLearnMoreLink}" target="_blank"
        @click="${this.onLearnMoreClick_}"
        ?hidden="${!this.enableEnhancedSiteControls}">
      <cr-icon icon="cr:help-outline"></cr-icon>
    </a>
  </extensions-toggle-row>
</div>

${this.getSortedHosts_().map(item => html`
  <div class="toggle-section site-toggle">
    <extensions-toggle-row ?checked="${this.isItemChecked_(item)}"
        class="host-toggle no-end-padding"
        ?disabled="${this.allowedOnAllHosts_()}"
        data-host="${item.host}" @change="${this.onHostAccessChanged_}">
      <div class="site-row">
        <div class="site-favicon"
            .style="background-image:${this.getFaviconUrl_(item.host)}"
            ?hidden="${!this.enableEnhancedSiteControls}">
        </div>
        <span>${item.host}</span>
      </div>
    </extensions-toggle-row>
  </div>`)}

${this.showMatchingRestrictedSitesDialog_ ? html`
  <extensions-restricted-sites-dialog
      .firstRestrictedSite="${this.matchingRestrictedSites_[0]}"
      @close="${this.onMatchingRestrictedSitesDialogClose_}">
  </extensions-restricted-sites-dialog>` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
