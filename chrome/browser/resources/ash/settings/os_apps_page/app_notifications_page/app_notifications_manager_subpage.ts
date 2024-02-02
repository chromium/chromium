// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'app-notifications-manager-subpage' is responsible for containing controls
 * for sending notifications for the apps.
 */

import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import './app_notification_row.js';

import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isRevampWayfindingEnabled} from '../../common/load_time_booleans.js';
import {App, AppNotificationsHandlerInterface, AppNotificationsObserverReceiver} from '../../mojom-webui/app_notification_handler.mojom-webui.js';
import {isAppInstalled} from '../os_apps_page.js';

import {getTemplate} from './app_notifications_manager_subpage.html.js';
import {getAppNotificationProvider} from './mojo_interface_provider.js';

export class SettingsAppNotificationsManagerSubpage extends PolymerElement {
  static get is() {
    return 'settings-app-notifications-manager-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      searchTerm: {
        type: String,
      },

      appList_: {
        type: Array,
        value: [],
      },

      /**
       * List of apps filtered through a search term.
       */
      filteredAppList_: {
        type: Array,
        value: () => [],
        computed: 'computeFilteredAppList_(appList_.*, searchTerm)',
      },
    };
  }

  searchTerm: string;
  private appList_: App[];
  private appNotificationsObserverReceiver_: AppNotificationsObserverReceiver|
      null;
  private filteredAppList_: App[];
  private mojoInterfaceProvider_: AppNotificationsHandlerInterface;

  constructor() {
    super();

    this.mojoInterfaceProvider_ = getAppNotificationProvider();

    /**
     * Receiver responsible for observing app notification events.
     */
    this.appNotificationsObserverReceiver_ = null;
  }

  override connectedCallback(): void {
    // This page only exists when revamp wayfinding is enabled.
    assert(isRevampWayfindingEnabled());

    super.connectedCallback();

    this.startObservingAppNotifications_();
    this.mojoInterfaceProvider_.getApps().then((result) => {
      this.appList_ = result.apps;
    });
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.appNotificationsObserverReceiver_!.$.close();
  }

  private startObservingAppNotifications_(): void {
    this.appNotificationsObserverReceiver_ =
        new AppNotificationsObserverReceiver(this);
    this.mojoInterfaceProvider_.addObserver(
        this.appNotificationsObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /** Override ash.settings.appNotification.onQuietModeChanged */
  onQuietModeChanged(): void {
    // Do nothing.
  }

  /** Override ash.settings.appNotification.onNotificationAppChanged */
  onNotificationAppChanged(updatedApp: App): void {
    // Using Polymer mutation methods do not properly handle splice updates with
    // object that have deep properties. Create and assign a copy list instead.
    const appList = Array.from(this.appList_);
    const foundIdx = this.appList_.findIndex(app => {
      return app.id === updatedApp.id;
    });
    if (isAppInstalled(updatedApp)) {
      if (foundIdx !== -1) {
        appList[foundIdx] = updatedApp;
      } else {
        appList.push(updatedApp);
      }
      this.appList_ = appList;
      return;
    }

    // Cannot have an app that is uninstalled prior to being installed.
    assert(foundIdx !== -1);
    // Uninstalled app found, remove it from the list.
    appList.splice(foundIdx, 1);
    this.appList_ = appList;
  }

  private computeFilteredAppList_(): App[] {
    if (this.appList_.length === 0) {
      return [];
    }

    if (!this.searchTerm) {
      return [...this.appList_];
    }

    const lowerCaseSearchTerm = this.searchTerm.toLocaleLowerCase();
    return this.appList_.filter(app => {
      assert(app.title);
      return app.title.toLocaleLowerCase().includes(lowerCaseSearchTerm);
    });
  }

  /**
   * A function used for sorting languages alphabetically.
   */
  private alphabeticalSort_(first: App, second: App): number {
    return first.title!.localeCompare(second.title!);
  }

  private isAppListEmpty_(appList: App[]): boolean {
    return appList.length === 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsAppNotificationsManagerSubpage.is]:
        SettingsAppNotificationsManagerSubpage;
  }
}

customElements.define(
    SettingsAppNotificationsManagerSubpage.is,
    SettingsAppNotificationsManagerSubpage);
