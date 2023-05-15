// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'app-notifications-page' is responsible for containing controls for
 * notifications of all apps.
 */

import './app_notification_row.js';
import '/shared/settings/controls/settings_toggle_button.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../../deep_linking_mixin.js';
import {recordSettingChange} from '../../metrics_recorder.js';
import {App, AppNotificationsHandlerInterface, AppNotificationsObserverReceiver} from '../../mojom-webui/app_notification_handler.mojom-webui.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {routes} from '../../os_settings_routes.js';
import {RouteObserverMixin} from '../../route_observer_mixin.js';
import {Route} from '../../router.js';
import {isAppInstalled} from '../os_apps_page.js';

import {getTemplate} from './app_notifications_subpage.html.js';
import {getAppNotificationProvider} from './mojo_interface_provider.js';

const AppNotificationsSubpageBase =
    DeepLinkingMixin(RouteObserverMixin(PolymerElement));

export class AppNotificationsSubpage extends AppNotificationsSubpageBase {
  static get is() {
    return 'settings-app-notifications-subpage';
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
       * Whether the App Badging toggle is visible.
       */
      showAppBadgingToggle_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showOsSettingsAppBadgingToggle');
        },
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
    };
  }

  prefs: {[key: string]: any};
  private appList_: App[];
  private appNotificationsObserverReceiver_: AppNotificationsObserverReceiver|
      null;
  private isDndEnabled_: boolean;
  private mojoInterfaceProvider_: AppNotificationsHandlerInterface;
  private showAppBadgingToggle_: boolean;
  private virtualDndPref_: chrome.settingsPrivate.PrefObject<boolean>;

  constructor() {
    super();

    this.mojoInterfaceProvider_ = getAppNotificationProvider();

    /**
     * Receiver responsible for observing app notification events.
     */
    this.appNotificationsObserverReceiver_ = null;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.startObservingAppNotifications_();
    this.mojoInterfaceProvider_.getQuietMode().then((result) => {
      this.isDndEnabled_ = result.enabled;
    });
    this.mojoInterfaceProvider_.getApps().then((result) => {
      this.appList_ = result.apps;
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.appNotificationsObserverReceiver_!.$.close();
  }

  override currentRouteChanged(route: Route) {
    // Does not apply to this page.
    if (route !== routes.APP_NOTIFICATIONS) {
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
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-app-notifications-subpage': AppNotificationsSubpage;
  }
}

customElements.define(AppNotificationsSubpage.is, AppNotificationsSubpage);
