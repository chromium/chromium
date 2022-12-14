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
import {getTemplate} from './app_list.html.js';
import {BrowserProxy} from './browser_proxy.js';

export interface ActionMenuModel {
  data: AppInfo;
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

  private addApp_(data: AppInfo) {
    this.push('apps_', data);
  }

  private removeApp_(data: AppInfo) {
    const index = this.apps_.findIndex(app => app.id === data.id);
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

  private onOpenInWindowItemClick_() {
    this.$.menu.close();
  }

  private onLaunchOnStartupItemClick_() {
    this.$.menu.close();
  }

  private onCreateShortcutItemClick_() {
    this.$.menu.close();
  }

  private onUninstallItemClick_() {
    if (this.selectedActionMenuModel_?.data.id) {
      BrowserProxy.getInstance().handler.uninstallApp(
          this.selectedActionMenuModel_?.data.id);
    }
    this.closeMenu_();
  }

  private onAppSettingsItemClick_() {
    this.$.menu.close();
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
