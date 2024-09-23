// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppItemElement} from './app_item.js';

export function getHtml(this: AppItemElement) {
  return html`<!--_html_template_start_-->
<div title="${this.appInfo.name}" aria-hidden="true"
    id="objectContainer">
  <div id="iconContainer">
    <img .src="${this.getIconUrl_()}" id="iconImage"
        alt="${this.appInfo.name}"
        aria-hidden="true"
        draggable="false">
    <img id="deprecatedIcon"
        src="chrome://resources/images/error_yellow900.svg"
        ?hidden="${!this.appInfo.isDeprecatedApp}">
  </div>
  <div id="textContainer"
      aria-hidden="true">${this.appInfo.name}</div>
</div>

<cr-action-menu id="menu" @mousedown="${this.onMenuMousedown_}"
    @click="${this.onMenuClick_}">
  <button id="showStorePage" class="dropdown-item"
      @click="${this.openStorePage_}"
      ?hidden="${this.isWebStoreLinkHidden_()}">
    $i18n{viewInWebStore}
  </button>
  <cr-checkbox id="openInWindow"
      class="dropdown-item label-first"
      @change="${this.onOpenInWindowItemChange_}"
      ?hidden="${this.isOpenInWindowHidden_()}"
      ?checked="${this.appInfo.openInWindow}" noink>
    $i18n{appWindowOpenLabel}
  </cr-checkbox>
  <cr-checkbox
      id="launchOnStartup"
      class="dropdown-item label-first"
      @click="${this.onLaunchOnStartupItemClick_}"
      ?hidden="${this.isLaunchOnStartupHidden_()}"
      ?checked="${this.isLaunchOnStartUp_()}"
      ?disabled="${this.isLaunchOnStartupDisabled_()}" noink>
    $i18n{appLaunchAtStartupLabel}
  </cr-checkbox>
  <button id="createShortcut"
      class="dropdown-item"
      @click="${this.onCreateShortcutItemClick_}"
      ?hidden="${this.isCreateShortcutHidden_()}">
    $i18n{createShortcutForAppLabel}
  </button>
  <button id="installLocally" class="dropdown-item"
      @click="${this.onInstallLocallyItemClick_}"
      ?hidden="${this.isInstallLocallyHidden_()}">
    $i18n{installLocallyLabel}
  </button>
  <hr>
  <button id="uninstall" class="dropdown-item"
      ?disabled="${!this.appInfo.mayUninstall}"
      ?hidden="${this.isUninstallHidden_()}"
      @click="${this.onUninstallItemClick_}">
    $i18n{uninstallAppLabel}
  </button>
  <button id="removeFromChrome" class="dropdown-item"
      ?disabled="${!this.appInfo.mayUninstall}"
      ?hidden="${this.isRemoveFromChromeHidden_()}"
      @click="${this.onUninstallItemClick_}">
    $i18n{removeAppLabel}
  </button>
  <button id="appSettings" class="dropdown-item"
      @click="${this.onAppSettingsItemClick_}"
      ?hidden="${this.isAppSettingsHidden_()}">
    $i18n{appSettingsLabel}
  </button>
</cr-action-menu>
<!--_html_template_end_-->`;
}
