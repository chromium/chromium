// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_item.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';

import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assert} from 'chrome://resources/js/assert_ts.js';

import {AppInfo, PageCallbackRouter} from './app_home.mojom-webui.js';
import {AppHomeUserAction, recordUserAction} from './app_home_utils.js';
import {AppItemElement} from './app_item.js';
import {getTemplate} from './app_list.html.js';
import {BrowserProxy} from './browser_proxy.js';

export interface ActionMenuModel {
  appItem: AppItemElement;
}

type MenuHandleEvent = CustomEvent<ActionMenuModel>;

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

      selectedAppItem: Object,
    };
  }

  private apps_: AppInfo[];
  private mojoEventTarget_: PageCallbackRouter;
  private listenerIds_: number[];
  // The app item that has the context menu click opened by user.
  private selectedAppItem: AppItemElement|null = null;

  constructor() {
    super();

    this.mojoEventTarget_ = BrowserProxy.getInstance().callbackRouter;

    BrowserProxy.getInstance().handler.getApps().then(result => {
      this.apps_ = result.appList;
    });
  }

  override ready() {
    super.ready();
    this.addEventListener('on-menu-open-triggered', this.switchActiveMenu_);
    this.addEventListener('on-menu-closed', this.clearActiveMenu_);
    recordUserAction(AppHomeUserAction.APP_HOME_INIT);
  }

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds_ = [
      this.mojoEventTarget_.addApp.addListener(this.addApp_.bind(this)),
      this.mojoEventTarget_.removeApp.addListener(this.removeApp_.bind(this)),
    ];
    document.addEventListener(
        'contextmenu', this.closeCurrentAppMenu.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => assert(this.mojoEventTarget_.removeListener(id)));
    this.listenerIds_ = [];
    document.removeEventListener(
        'contextmenu', this.closeCurrentAppMenu.bind(this));
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

  private closeCurrentAppMenu() {
    if (!this.selectedAppItem) {
      return;
    }
    this.selectedAppItem.closeContextMenu();
  }

  private clearActiveMenu_() {
    this.selectedAppItem = null;
  }

  // Close the menu on right click on a page.
  private switchActiveMenu_(event: MenuHandleEvent) {
    this.closeCurrentAppMenu();
    this.selectedAppItem = event.detail.appItem;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-list': AppListElement;
  }

  interface HTMLElementEventMap {
    'on-menu-open-triggered': MenuHandleEvent;
    'on-menu-closed': MenuHandleEvent;
  }
}

customElements.define(AppListElement.is, AppListElement);
