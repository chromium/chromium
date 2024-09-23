// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppElement} from './app.js';

export function getHtml(this: AppElement) {
  return html`<!--_html_template_start_-->
<cr-toolbar page-name="$i18n{title}" ?show-search="${this.showSearch_}"
    always-show-logo>
</cr-toolbar>
<div id="content" class="cr-centered-card-container">
  <div class="cr-row first" id="headerLine">
    <img id="title-icon" src="${this.iconUrl_}" aria-hidden="true">
    <h1 class="cr-title-text">${this.getTitle_()}</h1>
    <app-management-uninstall-button id="uninstall-button"
        .app="${this.app_}"
        uninstall-label="$i18n{appManagementUninstallLabel}"
        policy-label="$i18n{appManagementAppInstalledByPolicyLabel}">
    </app-management-uninstall-button>
  </div>
  <div class="permission-list">
    <app-management-run-on-os-login-item
        class="permission-card-row separated-row"
        login-mode-label="$i18n{appManagementRunOnOsLoginModeLabel}"
        .app="${this.app_}">
    </app-management-run-on-os-login-item>
    <app-management-window-mode-item
        class="permission-card-row separated-row"
        window-mode-label="$i18n{appManagementWindowModeLabel}"
        .app="${this.app_}">
    </app-management-window-mode-item>
    <app-management-permission-item
        class="permission-card-row separated-row"
        .app="${this.app_}"
        permission-label="$i18n{appManagementNotificationsLabel}"
        permission-type="kNotifications">
      ${this.shouldShowSystemNotificationsSettingsLink_() ? html`
        <localized-link slot="description"
            .localizedString=
                "${this.i18nAdvanced('appManagementNotificationsDescription')}"
            @link-clicked="${this.openNotificationsSystemSettings_}">
        </localized-link>
      ` : ''}
      </app-management-permission-item>
    <div id="permissions-card" class="permission-card-row">
      <div class="permission-section-header">
        <div class="header-text">${this.getPermissionsHeader_()}</div>
      </div>
      <div class="permission-list indented-permission-block">
        <app-management-permission-item class="subpermission-row"
            icon="app-management:location" .app="${this.app_}"
            permission-label="$i18n{appManagementLocationPermissionLabel}"
            permission-type="kLocation">
        </app-management-permission-item>
        <app-management-permission-item class="subpermission-row"
            icon="app-management:camera" .app="${this.app_}"
            permission-label="$i18n{appManagementCameraPermissionLabel}"
            permission-type="kCamera">
        </app-management-permission-item>
        <app-management-permission-item class="subpermission-row"
            icon="app-management:microphone" .app="${this.app_}"
            permission-label="$i18n{appManagementMicrophonePermissionLabel}"
            permission-type="kMicrophone">
        </app-management-permission-item>
      </div>
    </div>
    <app-management-file-handling-item
        class="permission-card-row separated-row"
        .app="${this.app_}">
    </app-management-file-handling-item>
    <app-management-app-content-item
        class="permission-card-row separated-row"
        .app="${this.app_}">
    </app-management-app-content-item>
    <app-management-more-permissions-item
        class="permission-card-row separated-row" .app="${this.app_}"
        more-permissions-label="$i18n{appManagementMorePermissionsLabel}">
    </app-management-more-permissions-item>
    <app-management-supported-links-item
        id="supportedLinksOption"
        class="permission-card-row"
        .app="${this.app_}"
        .apps="${this.apps_}">
    </app-management-supported-links-item>
  </div>
</div>
<!--_html_template_end_-->`;
}
