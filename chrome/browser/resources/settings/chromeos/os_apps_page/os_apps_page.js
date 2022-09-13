// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-apps-page' is the settings page containing app related settings.
 *
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared.css.js';
import '../../settings_shared.css.js';
import '../guest_os/guest_os_shared_usb_devices.js';
import '../guest_os/guest_os_shared_paths.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import './android_apps_subpage.js';
import './app_notifications_page/app_notifications_subpage.js';
import './app_management_page/app_management_page.js';
import './app_management_page/app_detail_view.js';
import 'chrome://resources/cr_components/app_management/uninstall_button.js';
import '../../controls/settings_dropdown_menu.js';

import {AppManagementEntryPoint, AppManagementEntryPointsHistogramName} from 'chrome://resources/cr_components/app_management/constants.js';
import {getAppIcon, getSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {AndroidAppsBrowserProxyImpl, AndroidAppsInfo} from './android_apps_browser_proxy.js';
import {AppManagementStoreClient, AppManagementStoreClientInterface} from './app_management_page/store_client.js';
import {getAppNotificationProvider} from './app_notifications_page/mojo_interface_provider.js';

/**
 * @param {!ash.settings.appNotification.mojom.App} app
 * @return {boolean}
 */
export function isAppInstalled(app) {
  switch (app.readiness) {
    case ash.settings.appNotification.mojom.Readiness.kReady:
    case ash.settings.appNotification.mojom.Readiness.kDisabledByBlocklist:
    case ash.settings.appNotification.mojom.Readiness.kDisabledByPolicy:
    case ash.settings.appNotification.mojom.Readiness.kDisabledByUser:
    case ash.settings.appNotification.mojom.Readiness.kTerminated:
      return true;
    case ash.settings.appNotification.mojom.Readiness.kUninstalledByUser:
    case ash.settings.appNotification.mojom.Readiness.kUninstalledByMigration:
    case ash.settings.appNotification.mojom.Readiness.kRemoved:
    case ash.settings.appNotification.mojom.Readiness.kUnknown:
      return false;
  }
  assertNotReached();
  return false;
}

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {AppManagementStoreClientInterface}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const OsSettingsAppsPageElementBase = mixinBehaviors(
    [
      AppManagementStoreClient,
      DeepLinkingBehavior,
      I18nBehavior,
      PrefsBehavior,
      RouteObserverBehavior,
    ],
    PolymerElement);

/** @polymer */
class OsSettingsAppsPageElement extends OsSettingsAppsPageElementBase {
  static get is() {
    return 'os-settings-apps-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * This object holds the playStoreEnabled and settingsAppAvailable
       * boolean.
       * @type {Object}
       */
      androidAppsInfo: Object,

      /**
       * If the Play Store app is available.
       * @type {boolean}
       */
      havePlayStoreApp: Boolean,

      /**
       * @type {string}
       */
      searchTerm: String,

      /**
       * Show ARC++ related settings and sub-page.
       * @type {boolean}
       */
      showAndroidApps: Boolean,

      /**
       * Show ARCVM Manage USB related settings and sub-page.
       * @type {boolean}
       */
      showArcvmManageUsb: Boolean,

      /**
       * Whether the App Notifications page should be shown.
       * @type {boolean}
       */
      showAppNotificationsRow_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showOsSettingsAppNotificationsRow');
        },
      },

      /**
       * Show Plugin VM shared folders sub-page.
       * @type {boolean}
       */
      showPluginVm: Boolean,

      /**
       * Show On startup settings and sub-page.
       * @type {boolean}
       */
      showStartup: Boolean,

      /** @private {!Map<string, string>} */
      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.APP_MANAGEMENT) {
            map.set(routes.APP_MANAGEMENT.path, '#appManagement');
          }
          if (routes.ANDROID_APPS_DETAILS) {
            map.set(
                routes.ANDROID_APPS_DETAILS.path,
                '#android-apps .subpage-arrow');
          }
          return map;
        },
      },

      /**
       * @type {App}
       * @private
       */
      app_: Object,

      /**
       * @type {!Array<!Object>}
       * @private
       */
      appsWithNotifications_: {
        type: Array,
        value: [],
      },

      /**
       * List of options for the on startup drop-down menu.
       * @type {!DropdownMenuOptionList}
       */
      onStartupOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {value: 1, name: loadTimeData.getString('onStartupAlways')},
            {value: 2, name: loadTimeData.getString('onStartupAskEveryTime')},
            {value: 3, name: loadTimeData.getString('onStartupDoNotRestore')},
          ];
        },
      },

      /** @private {boolean} */
      isDndEnabled_: {
        type: Boolean,
        value: false,
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kManageAndroidPreferences,
          Setting.kTurnOnPlayStore,
          Setting.kRestoreAppsAndPages,
        ]),
      },
    };
  }

  connectedCallback() {
    super.connectedCallback();

    this.watch('app_', state => {
      // Don't set `app_` to `null`, since it triggers Polymer
      // data bindings of <app-management-uninstall-button> which does not
      // accept `null`, use `undefined` instead.
      return getSelectedApp(state) || undefined;
    });

    /**
     * @private {!ash.settings.appNotification.mojom.AppNotificationsHandlerInterface}
     */
    this.mojoInterfaceProvider_ = getAppNotificationProvider();

    /**
     * @private {!ash.settings.appNotification.mojom.AppNotificationsObserverReceiver}
     */
    this.appNotificationsObserverReceiver_ =
        new ash.settings.appNotification.mojom.AppNotificationsObserverReceiver(
            /**
             * @type {!ash.settings.appNotification.mojom.
             * AppNotificationsObserverInterface}
             */
            (this));

    this.mojoInterfaceProvider_.addObserver(
        this.appNotificationsObserverReceiver_.$.bindNewPipeAndPassRemote());

    this.mojoInterfaceProvider_.getQuietMode().then((result) => {
      this.isDndEnabled_ = result.enabled;
    });
    this.mojoInterfaceProvider_.getApps().then((result) => {
      this.appsWithNotifications_ = result.apps;
    });
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.APPS) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_(app) {
    if (!app) {
      return '';
    }
    return getAppIcon(app);
  }

  /** @private */
  onClickAppManagement_() {
    chrome.metricsPrivate.recordEnumerationValue(
        AppManagementEntryPointsHistogramName,
        AppManagementEntryPoint.OS_SETTINGS_MAIN_PAGE,
        Object.keys(AppManagementEntryPoint).length);
    Router.getInstance().navigateTo(routes.APP_MANAGEMENT);
  }

  /** @private */
  onClickAppNotifications_() {
    Router.getInstance().navigateTo(routes.APP_NOTIFICATIONS);
  }

  /**
   * @param {!Event} event
   * @private
   */
  onEnableAndroidAppsTap_(event) {
    this.setPrefValue('arc.enabled', true);
    event.stopPropagation();
  }

  /**
   * @return {boolean}
   * @private
   */
  isEnforced_(pref) {
    return pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  }

  /** @private */
  onAndroidAppsSubpageTap_(event) {
    if (this.androidAppsInfo.playStoreEnabled) {
      Router.getInstance().navigateTo(routes.ANDROID_APPS_DETAILS);
    }
  }

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onManageAndroidAppsTap_(event) {
    // |event.detail| is the click count. Keyboard events will have 0 clicks.
    const isKeyboardAction = event.detail === 0;
    AndroidAppsBrowserProxyImpl.getInstance().showAndroidAppsSettings(
        isKeyboardAction);
  }

  /** Override ash.settings.appNotification.onNotificationAppChanged */
  onNotificationAppChanged(updatedApp) {
    const foundIdx = this.appsWithNotifications_.findIndex(app => {
      return app.id === updatedApp.id;
    });
    if (isAppInstalled(updatedApp)) {
      if (foundIdx !== -1) {
        this.splice('appsWithNotifications_', foundIdx, updatedApp);
        return;
      }
      this.push('appsWithNotifications_', updatedApp);
      return;
    }

    // Cannot have an app that is uninstalled prior to being installed.
    assert(foundIdx !== -1);
    // Uninstalled app found, remove it from the list.
    this.splice('appsWithNotifications_', foundIdx, 1);
  }

  /** Override ash.settings.appNotification.onQuietModeChanged */
  onQuietModeChanged(enabled) {
    this.isDndEnabled_ = enabled;
  }

  /**
   * @return {string}
   * @protected
   */
  getAppListCountDescription_() {
    return this.isDndEnabled_ ?
        this.i18n('appNotificationsDoNotDisturbDescription') :
        this.i18n(
            'appNotificationsCountDescription',
            this.appsWithNotifications_.length);
  }
}

customElements.define(OsSettingsAppsPageElement.is, OsSettingsAppsPageElement);
