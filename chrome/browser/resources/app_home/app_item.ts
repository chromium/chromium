// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppInfo, RunOnOsLoginMode} from './app_home.mojom-webui.js';
import {AppHomeUserAction, recordUserAction} from './app_home_utils.js';
import {getTemplate} from './app_item.html.js';
import {BrowserProxy} from './browser_proxy.js';
import {UserDisplayMode} from './user_display_mode.mojom-webui.js';

export interface AppItemElement {
  $: {
    menu: CrActionMenuElement,
  };
}

export class AppItemElement extends PolymerElement {
  static get is() {
    return 'app-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      appInfo: Object,
    };
  }

  appInfo: AppInfo;

  override ready() {
    super.ready();
    this.addEventListener('contextmenu', this.handleContextMenu_);
  }

  closeContextMenu() {
    if (!this.$.menu.open) {
      return;
    }
    this.$.menu.close();
    this.fire_('on-menu-closed', {appItem: this});
  }

  private handleContextMenu_(e: MouseEvent) {
    this.fire_('on-menu-open-triggered', {
      appItem: this,
    });

    this.$.menu.showAtPosition({top: e.clientY, left: e.clientX});
    recordUserAction(AppHomeUserAction.CONTEXT_MENU_TRIGGERED);

    e.preventDefault();
    e.stopPropagation();
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  // The CrActionMenuElement is a modal that does not listen to any other
  // events other than mousedown on right click when it is open. This allows
  // us to listen to changes on the dom even when the menu is showing.
  private onMenuMousedown_(e: Event) {
    // Do not listen to the mousedown event if not triggered from a
    // CrActionMenuElement, i.e. one without a dialog element covering the dom.
    if ((e.composedPath()[0] as HTMLElement).tagName !== 'DIALOG') {
      return;
    }

    this.closeContextMenu();
  }

  private isLocallyInstalled_() {
    return this.appInfo.isLocallyInstalled;
  }

  private isLaunchOnStartupHidden_() {
    return !this.appInfo.mayShowRunOnOsLoginMode;
  }

  private isLaunchOnStartupDisabled_() {
    return !this.appInfo.mayToggleRunOnOsLoginMode;
  }

  private isLaunchOnStartUp_() {
    return (this.appInfo.runOnOsLoginMode !== RunOnOsLoginMode.kNotRun);
  }

  private onOpenInWindowItemClick_() {
    if (this.appInfo.openInWindow) {
      BrowserProxy.getInstance().handler.setUserDisplayMode(
          this.appInfo.id, UserDisplayMode.kBrowser);
      recordUserAction(AppHomeUserAction.OPEN_IN_WINDOW_UNCHECKED);
    } else {
      BrowserProxy.getInstance().handler.setUserDisplayMode(
          this.appInfo.id, UserDisplayMode.kStandalone);
      recordUserAction(AppHomeUserAction.OPEN_IN_WINDOW_CHECKED);
    }
    this.closeContextMenu();
  }

  // Changing the app's launch mode.
  private onLaunchOnStartupItemClick_() {
    if (this.isLaunchOnStartupDisabled_()) {
      return;
    }

    if (this.isLaunchOnStartUp_()) {
      BrowserProxy.getInstance().handler.setRunOnOsLoginMode(
          this.appInfo.id, RunOnOsLoginMode.kNotRun);
      recordUserAction(AppHomeUserAction.LAUNCH_AT_STARTUP_UNCHECKED);
    } else {
      BrowserProxy.getInstance().handler.setRunOnOsLoginMode(
          this.appInfo.id, RunOnOsLoginMode.kWindowed);
      recordUserAction(AppHomeUserAction.LAUNCH_AT_STARTUP_CHECKED);
    }
    this.closeContextMenu();
  }

  private onCreateShortcutItemClick_() {
    if (this.appInfo.id) {
      BrowserProxy.getInstance().handler.createAppShortcut(this.appInfo.id);
      recordUserAction(AppHomeUserAction.CREATE_SHORTCUT);
    }
    this.closeContextMenu();
  }

  private onInstallLocallyItemClick_() {
    if (this.appInfo.id) {
      BrowserProxy.getInstance().handler.installAppLocally(this.appInfo.id);
      recordUserAction(AppHomeUserAction.INSTALL_APP_LOCALLY);
    }
    this.closeContextMenu();
  }

  private onUninstallItemClick_() {
    if (this.appInfo.id) {
      BrowserProxy.getInstance().handler.uninstallApp(this.appInfo.id);
      recordUserAction(AppHomeUserAction.UNINSTALL);
    }
    this.closeContextMenu();
  }

  private onAppSettingsItemClick_() {
    if (this.appInfo.id) {
      BrowserProxy.getInstance().handler.showAppSettings(this.appInfo.id);
      recordUserAction(AppHomeUserAction.OPEN_APP_SETTINGS);
    }
    this.closeContextMenu();
  }

  private getIconUrl_() {
    const url = new URL(this.appInfo.iconUrl.url);
    // For web app, the backend serves grayscale image when the app is not
    // locally installed automatically and doesn't recognize this query param,
    // but we add a query param here to force browser to refetch the image.
    if (!this.isLocallyInstalled_()) {
      url.searchParams.append('grayscale', 'true');
    }
    return url;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-item': AppItemElement;
  }
}

customElements.define(AppItemElement.is, AppItemElement);