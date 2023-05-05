// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_item.js';
import './app_home_empty_page.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';

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

      selectedAppItem_: Object,
    };
  }

  private apps_: AppInfo[];
  private mojoEventTarget_: PageCallbackRouter;
  private listenerIds_: number[];
  // The app item that has the context menu click opened by user.
  private selectedAppItem_: AppItemElement|null = null;
  private boundKeydownListener_: any;
  private boundContextMenuListener_: any;

  constructor() {
    super();

    this.mojoEventTarget_ = BrowserProxy.getInstance().callbackRouter;

    BrowserProxy.getInstance().handler.getApps().then(result => {
      this.apps_ = result.appList;
    });

    this.boundKeydownListener_ = this.handleKeyDown.bind(this);
    this.boundContextMenuListener_ = this.closeCurrentAppMenu.bind(this);
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
    document.addEventListener('contextmenu', this.boundContextMenuListener_);
    document.addEventListener('keydown', this.boundKeydownListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => assert(this.mojoEventTarget_.removeListener(id)));
    this.listenerIds_ = [];
    document.removeEventListener('contextmenu', this.boundContextMenuListener_);
    document.removeEventListener('keydown', this.boundKeydownListener_);
  }

  private handleKeyDown(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      this.launchFocusedApp();
    } else if (['ArrowRight', 'ArrowLeft', 'ArrowUp', 'ArrowDown'].includes(
                   e.key)) {
      this.handleNavigateWithArrows(e);
    } else if (e.key === 'F10' && e.shiftKey) {
      this.launchContextMenuForFocusedApp();
      e.preventDefault();
      e.stopPropagation();
    }
  }

  private launchFocusedApp() {
    const activeElementId = this.shadowRoot!.activeElement?.id;
    if (activeElementId !== undefined &&
        this.apps_.some(app => activeElementId === app.id)) {
      BrowserProxy.getInstance().handler.launchApp(activeElementId!, null);
    }
  }

  private launchContextMenuForFocusedApp() {
    const activeElementId = this.shadowRoot!.activeElement?.id;
    if (!activeElementId) {
      return;
    }

    const currIndex = this.apps_.findIndex(app => activeElementId === app.id);
    if (currIndex < 0) {
      return;
    }

    const appElement =
        (this.shadowRoot!.getElementById('container')
             ?.querySelector('#' + this.apps_[currIndex].id) as HTMLElement);
    if (!appElement) {
      return;
    }

    // Dispatch the contextmenu event on the focused element.
    appElement.dispatchEvent(new CustomEvent('contextmenu'));
  }

  // Capture arrow key events to focus on apps and navigate the apps as a grid.
  private handleNavigateWithArrows(e: KeyboardEvent) {
    const numApps = this.apps_.length;
    const cssProps =
        window.getComputedStyle(this.shadowRoot!.getElementById('container')!);
    const numColumns: number =
        cssProps!.getPropertyValue('grid-template-columns')!.split(' ').length;
    const keyActions = {
      ArrowRight: 1,
      ArrowLeft: -1,
      ArrowUp: -numColumns,
      ArrowDown: numColumns,
    };

    if (!(e.key in keyActions) || numApps === 0) {
      return;
    }

    const activeElementId = this.shadowRoot!.activeElement?.id;
    if (!activeElementId) {
      (this.shadowRoot!.getElementById('container')
           ?.querySelector('#' + this.apps_[0].id) as HTMLElement)!.focus();
      return;
    }

    const currIndex = this.apps_.findIndex(app => activeElementId === app.id);

    let nextIndex: number;
    if (currIndex === -1) {
      nextIndex = 0;
    } else if (
        currIndex + keyActions[e.key as keyof typeof keyActions] >= 0 &&
        currIndex + keyActions[e.key as keyof typeof keyActions] < numApps) {
      nextIndex = currIndex + keyActions[e.key as keyof typeof keyActions];
    } else {
      nextIndex = currIndex;
    }

    (this.shadowRoot!.getElementById('container')
         ?.querySelector('#' + this.apps_[nextIndex].id) as
     HTMLElement)!.focus();
  }

  private addApp_(appInfo: AppInfo) {
    const currIndex = this.apps_.findIndex(app => app.id === appInfo.id);
    if (currIndex !== -1) {
      this.set(`apps_.${currIndex}`, appInfo);
    } else {
      const newIndex = this.apps_.findIndex(app => app.name > appInfo.name);
      if (newIndex === -1) {
        this.push('apps_', appInfo);
        return;
      }
      this.splice('apps_', newIndex, 0, appInfo);
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
    if (!this.selectedAppItem_) {
      return;
    }
    this.selectedAppItem_.closeContextMenu();
  }

  private clearActiveMenu_() {
    this.selectedAppItem_ = null;
  }

  // Close the menu on right click on a page.
  private switchActiveMenu_(event: MenuHandleEvent) {
    this.closeCurrentAppMenu();
    this.selectedAppItem_ = event.detail.appItem;
  }

  private notLocallyInstalledString_(installed: boolean, i18nString: string) {
    if (!installed) {
      return ' (' + i18nString + ')';
    }
    return '';
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
