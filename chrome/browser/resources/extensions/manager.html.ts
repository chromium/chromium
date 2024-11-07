// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsManagerElement} from './manager.js';

export function getHtml(this: ExtensionsManagerElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<extensions-drop-overlay ?drag-enabled="${this.inDevMode}">
</extensions-drop-overlay>
<extensions-toolbar id="toolbar" ?in-dev-mode="${this.inDevMode}"
    ?can-load-unpacked="${this.canLoadUnpacked}"
    ?is-child-account="${this.isChildAccount_}"
    ?dev-mode-controlled-by-policy="${this.devModeControlledByPolicy}"
    .delegate="${this.delegate}"
    @cr-toolbar-menu-click="${this.onMenuButtonClick_}"
    @search-changed="${this.onFilterChanged_}"
    .extensions="${this.extensions_}"
    ?narrow="${this.narrow_}"
    @narrow-changed="${this.onNarrowChanged_}">
</extensions-toolbar>
${this.showDrawer_ ? html`
  <cr-drawer id="drawer" heading="$i18n{toolbarTitle}"
      align="$i18n{textdirection}" @close="${this.onDrawerClose_}">
    <div slot="body">
      <extensions-sidebar @close-drawer="${this.onCloseDrawer_}"
          enable-enhanced-site-controls="${this.enableEnhancedSiteControls}">
      </extensions-sidebar>
    </div>
  </cr-drawer>` : ''}
<div id="container">
  <div id="left" ?hidden="${this.narrow_}">
    <extensions-sidebar @close-drawer="${this.onCloseDrawer_}"
        ?enable-enhanced-site-controls="${this.enableEnhancedSiteControls}">
    </extensions-sidebar>
  </div>
  <cr-view-manager id="viewManager" role="main">
    <extensions-item-list id="items-list" .delegate="${this.delegate}"
        ?in-dev-mode="${this.inDevMode}"
        ?is-mv2-deprecation-notice-dismissed=
            "${this.isMv2DeprecationNoticeDismissed}"
        .filter="${this.filter}" ?hidden="${!this.didInitPage_}" slot="view"
        .apps="${this.apps_}" .extensions="${this.extensions_}"
        @show-install-warnings="${this.onShowInstallWarnings_}">
    </extensions-item-list>
    <cr-lazy-render-lit id="details-view" .template="${() => html`
        <extensions-detail-view .delegate="${this.delegate}" slot="view"
            ?in-dev-mode="${this.inDevMode}"
            ?enable-enhanced-site-controls="${this.enableEnhancedSiteControls}"
            ?from-activity-log="${this.fromActivityLog_}"
            ?show-activity-log="${this.showActivityLog}"
            ?incognito-available="${this.incognitoAvailable_}"
            .data="${this.detailViewItem_}">
        </extensions-detail-view>`}">
    </cr-lazy-render-lit>
    <cr-lazy-render-lit id="activity-log" .template="${() => html`
        <extensions-activity-log .delegate="${this.delegate}" slot="view"
            .extensionInfo="${this.activityLogItem_}">
        </extensions-activity-log>`}">
    </cr-lazy-render-lit>
    <cr-lazy-render-lit id="site-permissions" .template="${() => html`
        <extensions-site-permissions .delegate="${this.delegate}" slot="view"
            .extensions="${this.extensions_}"
            ?enable-enhanced-site-controls="${this.enableEnhancedSiteControls}">
        </extensions-site-permissions>`}">
    </cr-lazy-render-lit>
    <cr-lazy-render-lit id="site-permissions-by-site" .template="${() => html`
        <extensions-site-permissions-by-site .delegate="${this.delegate}"
            slot="view" .extensions="${this.extensions_}">
        </extensions-site-permissions-by-site>`}">
    </cr-lazy-render-lit>
    <cr-lazy-render-lit id="keyboard-shortcuts" .template="${() => html`
        <extensions-keyboard-shortcuts .delegate="${this.delegate}" slot="view"
            .items="${this.extensions_}">
        </extensions-keyboard-shortcuts>`}">
    </cr-lazy-render-lit>
    <cr-lazy-render-lit id="error-page" .template="${() => html`
        <extensions-error-page .data="${this.errorPageItem_}" slot="view"
            .delegate="${this.delegate}" ?in-dev-mode="${this.inDevMode}">
        </extensions-error-page>`}">
    </cr-lazy-render-lit>
  </cr-view-manager>
  <div id="right" ?hidden="${this.narrow_}"></div>
</div>
${this.showOptionsDialog_ ? html`
  <extensions-options-dialog id="options-dialog"
      @close="${this.onOptionsDialogClose_}">
  </extensions-options-dialog>` : ''}
${this.showLoadErrorDialog_ ? html`
  <extensions-load-error id="load-error" .delegate="${this.delegate}"
      @close="${this.onLoadErrorDialogClose_}">
  </extensions-load-error>`: ''}
${this.showInstallWarningsDialog_ ? html`
  <extensions-install-warnings-dialog
      @close="${this.onInstallWarningsDialogClose_}"
      .installWarnings="${this.installWarnings_}">
  </extensions-install-warnings-dialog>` : ''}
<cr-toast-manager></cr-toast-manager>
<!--_html_template_end_-->`;
  // clang-format on
}
