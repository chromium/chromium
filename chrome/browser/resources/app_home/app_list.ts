// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_item.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';

import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assert} from 'chrome://resources/js/assert_ts.js';

import {AppInfo, PageCallbackRouter, RunOnOsLoginMode} from './app_home.mojom-webui.js';
import {getTemplate} from './app_list.html.js';
import {BrowserProxy} from './browser_proxy.js';
import {UserDisplayMode} from './user_display_mode.mojom-webui.js';

export interface ActionMenuModel {
  appInfo: AppInfo;
  event: MouseEvent;
}

type OpenMenuEvent = CustomEvent<ActionMenuModel>;

export interface AppListElement {
  $: {
    menu: CrActionMenuElement,
  };
}

export class AppListElement extends PolymerElement {
  static get is() {
    return 'app-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      apps_: {
        type: Array,
        value() {
          return [];
        },
      },

      selectedActionMenuModel_: Object,
    };
  }

  private apps_: AppInfo[];
  private mojoEventTarget_: PageCallbackRouter;
  private listenerIds_: number[];
  // The app context menu that's currently click opened by user.
  private selectedActionMenuModel_: ActionMenuModel|null = null;

  constructor() {
    super();

    this.mojoEventTarget_ = BrowserProxy.getInstance().callbackRouter;

    BrowserProxy.getInstance().handler.getApps().then(result => {
      this.apps_ = result.appList;
    });
  }

  override ready() {
    super.ready();
    this.addEventListener('open-menu', this.onOpenMenu_);
  }

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds_ = [
      this.mojoEventTarget_.addApp.addListener(this.addApp_.bind(this)),
      this.mojoEventTarget_.removeApp.addListener(this.removeApp_.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => assert(this.mojoEventTarget_.removeListener(id)));
    this.listenerIds_ = [];
  }

  private addApp_(appInfo: AppInfo) {
    const index = this.apps_.findIndex(app => app.id === appInfo.id);
    if (index !== -1) {
      this.set(`apps_.${index}`, appInfo);
    } else {
      this.push('apps_', appInfo);
    }
  }

  private removeApp_(appInfo: AppInfo) {
    const index = this.apps_.findIndex(app => app.id === appInfo.id);
    // We gracefully handle item not found case because:
    // 1.if the async getApps() returns later than an uninstall event,
    // it should gracefully handles that and ignores that uninstall event,
    // the getApps() will return the list without the app later.
    // 2.If an uninstall event gets fired for an app that's somehow not in
    // the list of apps shown in current page, it's none of the concern
    // for this page to remove it.
    if (index !== -1) {
      this.splice('apps_', index, 1);
    }
  }

  private isLocallyInstalled_() {
    return this.selectedActionMenuModel_ ?
        this.selectedActionMenuModel_.appInfo.isLocallyInstalled :
        false;
  }

  private isLaunchOnStartupHidden_() {
    return this.selectedActionMenuModel_ ?
        !this.selectedActionMenuModel_.appInfo.mayShowRunOnOsLoginMode :
        true;
  }

  private isLaunchOnStartupDisabled_() {
    return this.selectedActionMenuModel_ ?
        !this.selectedActionMenuModel_.appInfo.mayToggleRunOnOsLoginMode :
        true;
  }

  private isLaunchOnStartUp_() {
    return this.selectedActionMenuModel_ ?
        (this.selectedActionMenuModel_.appInfo.runOnOsLoginMode !==
         RunOnOsLoginMode.kNotRun) :
        false;
  }

  private onOpenInWindowItemClick_() {
    if (this.selectedActionMenuModel_) {
      const appInfo = this.selectedActionMenuModel_.appInfo;
      if (appInfo.openInWindow) {
        BrowserProxy.getInstance().handler.setUserDisplayMode(
            appInfo.id, UserDisplayMode.kBrowser);
      } else {
        BrowserProxy.getInstance().handler.setUserDisplayMode(
            appInfo.id, UserDisplayMode.kStandalone);
      }
    }
    this.closeMenu_();
  }

  // Changing the app's launch mode.
  private onLaunchOnStartupItemClick_() {
    if (this.selectedActionMenuModel_) {
      const appInfo = this.selectedActionMenuModel_.appInfo;
      if (this.isLaunchOnStartUp_()) {
        BrowserProxy.getInstance().handler.setRunOnOsLoginMode(
            appInfo.id, RunOnOsLoginMode.kNotRun);
      } else {
        BrowserProxy.getInstance().handler.setRunOnOsLoginMode(
            appInfo.id, RunOnOsLoginMode.kWindowed);
      }
    }
    this.closeMenu_();
  }

  private onCreateShortcutItemClick_() {
    if (this.selectedActionMenuModel_?.appInfo.id) {
      BrowserProxy.getInstance().handler.createAppShortcut(
          this.selectedActionMenuModel_?.appInfo.id);
    }
    this.closeMenu_();
  }

  private onInstallLocallyItemClick_() {
    if (this.selectedActionMenuModel_?.appInfo.id) {
      BrowserProxy.getInstance().handler.installAppLocally(
          this.selectedActionMenuModel_?.appInfo.id);
    }
    this.closeMenu_();
  }

  private onUninstallItemClick_() {
    if (this.selectedActionMenuModel_?.appInfo.id) {
      BrowserProxy.getInstance().handler.uninstallApp(
          this.selectedActionMenuModel_?.appInfo.id);
    }
    this.closeMenu_();
  }

  private onAppSettingsItemClick_() {
    if (this.selectedActionMenuModel_?.appInfo.id) {
      BrowserProxy.getInstance().handler.showAppSettings(
          this.selectedActionMenuModel_?.appInfo.id);
    }
    this.closeMenu_();
  }

  private onOpenMenu_(event: OpenMenuEvent) {
    this.selectedActionMenuModel_ = event.detail;
    this.$.menu.showAtPosition({
      top: event.detail.event.clientY,
      left: event.detail.event.clientX,
    });
  }

  private closeMenu_() {
    this.selectedActionMenuModel_ = null;
    this.$.menu.close();
  }
}

declare global {
  interface HTMLElementEventMap {
    'open-menu': OpenMenuEvent;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-list': AppListElement;
  }
}

customElements.define(AppListElement.is, AppListElement);
