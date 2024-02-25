// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'app-notifications-page' is responsible for containing controls for
 * notifications of all apps.
 */

import './app_notification_row.js';
import '../../controls/settings_toggle_button.js';

import {isPermissionEnabled} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../../common/deep_linking_mixin.js';
import {isRevampWayfindingEnabled} from '../../common/load_time_booleans.js';
import {RouteOriginMixin} from '../../common/route_origin_mixin.js';
import {recordSettingChange} from '../../metrics_recorder.js';
import {App, AppNotificationsHandlerInterface, AppNotificationsObserverReceiver} from '../../mojom-webui/app_notification_handler.mojom-webui.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../../router.js';
import {isAppInstalled} from '../os_apps_page.js';

import {getTemplate} from './app_notifications_subpage.html.js';
import {getAppNotificationProvider} from './mojo_interface_provider.js';

const AppNotificationsSubpageBase =
    DeepLinkingMixin(RouteOriginMixin(I18nMixin(PolymerElement)));

export class AppNotificationsSubpage extends AppNotificationsSubpageBase {
  static get is() {
    return 'settings-app-notifications-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Reflects the Do Not Disturb property.
       */
      isDndEnabled_: {
        type: Boolean,
        value: false,
        notify: true,
        reflectToAttribute: true,
      },

      /**
       * A virtual pref to reflect the Do Not Disturb state.
       */
      virtualDndPref_: {
        type: Object,
        computed: 'getVirtualDndPref_(isDndEnabled_)',
      },

      appList_: {
        type: Array,
        value: [],
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kDoNotDisturbOnOff,
          Setting.kAppBadgingOnOff,
        ]),
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value() {
          return isRevampWayfindingEnabled();
        },
        readOnly: true,
      },
    };
  }

  prefs: {[key: string]: any};
  private appList_: App[];
  private appNotificationsObserverReceiver_: AppNotificationsObserverReceiver|
      null;
  private isDndEnabled_: boolean;
  private mojoInterfaceProvider_: AppNotificationsHandlerInterface;
  private virtualDndPref_: chrome.settingsPrivate.PrefObject<boolean>;
  private isRevampWayfindingEnabled_: boolean;

  constructor() {
    super();

    this.mojoInterfaceProvider_ = getAppNotificationProvider();

    /**
     * Receiver responsible for observing app notification events.
     */
    this.appNotificationsObserverReceiver_ = null;

    /** RouteOriginMixin override */
    this.route = routes.APP_NOTIFICATIONS;
  }

  override connectedCallback(): void {
    super.connectedCallback();
    this.startObservingAppNotifications_();
    this.mojoInterfaceProvider_.getQuietMode().then((result) => {
      this.isDndEnabled_ = result.enabled;
    });
    this.mojoInterfaceProvider_.getApps().then((result) => {
      this.appList_ = result.apps;
    });
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.appNotificationsObserverReceiver_!.$.close();
  }

  override ready(): void {
    super.ready();

    this.addFocusConfig(
        routes.APP_NOTIFICATIONS_MANAGER, '#appNotificationsManagerRow');
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    super.currentRouteChanged(newRoute, oldRoute);

    // Does not apply to this page.
    if (newRoute !== routes.APP_NOTIFICATIONS) {
      return;
    }
    this.attemptDeepLink();
  }

  private startObservingAppNotifications_(): void {
    this.appNotificationsObserverReceiver_ =
        new AppNotificationsObserverReceiver(this);
    this.mojoInterfaceProvider_.addObserver(
        this.appNotificationsObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /** Override ash.settings.appNotification.onQuietModeChanged */
  onQuietModeChanged(enabled: boolean): void {
    this.isDndEnabled_ = enabled;
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

  private setQuietMode_(): void {
    this.isDndEnabled_ = !this.isDndEnabled_;
    this.mojoInterfaceProvider_.setQuietMode(this.isDndEnabled_);
    recordSettingChange(
        Setting.kDoNotDisturbOnOff, {boolValue: this.isDndEnabled_});
  }

  private getVirtualDndPref_(): chrome.settingsPrivate.PrefObject<boolean> {
    return {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: this.isDndEnabled_,
    };
  }

  /**
   * A function used for sorting languages alphabetically.
   */
  private alphabeticalSort_(first: App, second: App): number {
    return first.title!.localeCompare(second.title!);
  }

  private onBrowserSettingsLinkClicked_(event: CustomEvent<{event: Event}>):
      void {
    // Prevent the default link click behavior.
    event.detail.event.preventDefault();

    // Programmatically open browser settings.
    this.mojoInterfaceProvider_.openBrowserNotificationSettings();
  }

  private getNotificationsCountSublabel_(): string {
    let numNotificationsEnabled = 0;
    this.appList_.forEach((app: App) => {
      if (isPermissionEnabled(app.notificationPermission.value)) {
        numNotificationsEnabled++;
      }
    });

    return this.i18n(
        'appNotificationsManagerSublabel', numNotificationsEnabled,
        this.appList_.length);
  }

  private onClickAppNotifications_(): void {
    Router.getInstance().navigateTo(routes.APP_NOTIFICATIONS_MANAGER);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AppNotificationsSubpage.is]: AppNotificationsSubpage;
  }
}

customElements.define(AppNotificationsSubpage.is, AppNotificationsSubpage);
