// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsSafetyHubNotificationPermissionsModuleElement} from './notification_permissions_module.js';

export function getHtml(
    this: SettingsSafetyHubNotificationPermissionsModuleElement) {
  return html`<!--_html_template_start_-->
<settings-safety-hub-module
    id="module"
    animated
    @sh-module-item-button-click="${this.onShModuleItemButtonClick_}"
    @sh-module-more-action-button-click="${this.onShModuleMoreActionButtonClick_}"
    header="${this.headerString_}"
    .subheader="${this.subheaderString_}"
    button-aria-label-id="safetyHubNotificationPermissionReviewDontAllowAriaLabel"
    button-icon="cr20:block"
    button-tooltip-text="$i18n{safetyHubNotificationPermissionReviewDontAllowLabel}"
    more-action-visible
    more-button-aria-label-id="safetyHubNotificationPermissionReviewMoreActionsAriaLabel"
    .sites="${this.sites_}">
  <div slot="button-container">
    <cr-button id="blockAllButton" @click="${this.onBlockAllClick_}"
        ?hidden="${this.shouldShowCompletionInfo_}">
      $i18n{safetyHubNotificationPermissionReviewBlockAllLabel}
    </cr-button>
    <cr-icon-button id="moreActionButton" class="icon-more-vert"
        @click="${this.onHeaderMoreActionClick_}"
        ?hidden="${this.shouldShowCompletionInfo_}" title="$i18n{moreActions}">
    </cr-icon-button>
    <cr-icon-button id="bulkUndoButton" iron-icon="settings20:undo"
        @click="${this.onUndoClick_}" ?hidden="${!this.shouldShowCompletionInfo_}"
        @focus="${this.onBulkUndoButtonFocus_}"
        @mouseenter="${this.onBulkUndoButtonMouseenter_}"
        aria-label="$i18n{safetyHubNotificationPermissionReviewUndo}">
    </cr-icon-button>
  </div>
</settings-safety-hub-module>
<cr-action-menu id="actionMenu" role-description="$i18n{menu}">
  <button class="dropdown-item" id="ignore" @click="${this.onIgnoreClick_}"
      aria-label="${this.getIgnoreAriaLabelForOrigins_()}">
    $i18n{safetyHubNotificationPermissionReviewIgnoreLabel}
  </button>
  <button class="dropdown-item" id="reset" @click="${this.onResetClick_}"
      aria-label="${this.getResetAriaLabelForOrigins_()}">
    $i18n{safetyHubNotificationPermissionReviewResetLabel}
  </button>
</cr-action-menu>
<cr-action-menu id="headerActionMenu" role-description="$i18n{menu}">
  <button class="dropdown-item" id="goToSettings"
      @click="${this.onGoToSettingsClick_}">
    $i18n{safetyHubGoNotificationSettingsItem}
  </button>
</cr-action-menu>
<cr-toast id="undoToast" duration="5000">
  <div id="undoNotification">${this.toastText_}</div>
  <cr-button id="toastUndoButton" @click="${this.onUndoClick_}"
      aria-label="$i18n{safetyHubNotificationPermissionReviewUndo}">
    $i18n{safetyHubNotificationPermissionReviewUndo}
  </cr-button>
</cr-toast>
<cr-tooltip fit-to-visible-bounds manual-mode position="top" offset="3">
  $i18n{safetyHubNotificationPermissionReviewUndo}
</cr-tooltip>
<!--_html_template_end_-->`;
}
