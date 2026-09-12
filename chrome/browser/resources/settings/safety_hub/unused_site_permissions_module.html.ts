// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsSafetyHubUnusedSitePermissionsModuleElement} from './unused_site_permissions_module.js';

export function getHtml(
    this: SettingsSafetyHubUnusedSitePermissionsModuleElement) {
  return html`<!--_html_template_start_-->
<settings-safety-hub-module
    id="module"
    animated
    @sh-module-item-button-click="${this.onShModuleItemButtonClick_}"
    header="${this.headerString_}"
    .subheader="${this.subheaderString_}"
    button-aria-label-id="safetyHubUnusedSitePermissionsAllowAgainAriaLabel"
    button-icon="settings20:undo"
    button-tooltip-text="$i18n{safetyHubUnusedSitePermissionsAllowAgainLabel}"
    .sites="${this.sites_}">
  <div slot="button-container">
    <cr-button id="gotItButton" @click="${this.onGotItClick_}"
        ?hidden="${this.shouldShowCompletionInfo_}"
        aria-description="${this.subheaderString_}">
      $i18n{safetyHubUnusedSitePermissionsGotItLabel}
    </cr-button>
    <cr-icon-button id="moreActionButton" class="icon-more-vert"
        @click="${this.onMoreActionClick_}"
        ?hidden="${this.shouldShowCompletionInfo_}"
        title="$i18n{moreActions}"
        aria-description="${this.headerString_}">
    </cr-icon-button>
    <cr-icon-button id="bulkUndoButton" iron-icon="settings20:undo"
        @click="${this.onUndoClick_}"
        ?hidden="${!this.shouldShowCompletionInfo_}"
        @focus="${this.onBulkUndoButtonFocus_}"
        @mouseenter="${this.onBulkUndoButtonMouseenter_}"
        aria-label="$i18n{safetyHubUnusedSitePermissionsUndoLabel}"
        aria-description="${this.headerString_}">
    </cr-icon-button>
  </div>
</settings-safety-hub-module>
<cr-toast id="undoToast" duration="5000">
  <div>${this.toastText_}</div>
  <cr-button id="toastUndoButton" @click="${this.onUndoClick_}">
    $i18n{safetyHubUnusedSitePermissionsUndoLabel}
  </cr-button>
</cr-toast>
<cr-action-menu id="headerActionMenu" role-description="$i18n{menu}">
  <button class="dropdown-item" id="goToSettings"
      @click="${this.onGoToSettingsClick_}">
    $i18n{safetyHubGoSiteSettingsItem}
  </button>
</cr-action-menu>
<cr-tooltip fit-to-visible-bounds manual-mode position="top" offset="3">
  $i18n{safetyHubUnusedSitePermissionsUndoLabel}
</cr-tooltip>
<!--_html_template_end_-->`;
}
