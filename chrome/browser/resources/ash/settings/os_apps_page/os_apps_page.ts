// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-apps-page' is the settings page containing app related settings.
 *
 */
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_pref_indicator.js';
import '../controls/settings_dropdown_menu.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import '../settings_shared.css.js';
import '../guest_os/guest_os_shared_usb_devices.js';
import '../guest_os/guest_os_shared_paths.js';
import './android_apps_subpage.js';
import './app_notifications_page/app_notifications_subpage.js';
import './app_management_page/app_management_page.js';
import './app_management_page/app_detail_view.js';
import './app_management_page/uninstall_button.js';
import './app_parental_controls/app_setup_pin_dialog.js';
import './app_parental_controls/app_verify_pin_dialog.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppManagementEntryPoint, AppManagementEntryPointsHistogramName} from 'chrome://resources/cr_components/app_management/constants.js';
import {getAppIcon, getSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppManagementStoreMixin} from '../common/app_management/store_mixin.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {androidAppsVisible, isAppParentalControlsFeatureAvailable, isArcVmEnabled, isPlayStoreAvailable, isPluginVmAvailable, isRevampWayfindingEnabled, shouldShowStartup} from '../common/load_time_booleans.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import {App as AppWithNotifications, AppNotificationsHandlerInterface, AppNotificationsObserverReceiver, Readiness} from '../mojom-webui/app_notification_handler.mojom-webui.js';
import {AppParentalControlsHandlerInterface} from '../mojom-webui/app_parental_controls_handler.mojom-webui.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {AndroidAppsBrowserProxyImpl, AndroidAppsInfo} from './android_apps_browser_proxy.js';
import {getAppNotificationProvider} from './app_notifications_page/mojo_interface_provider.js';
import {getAppParentalControlsProvider} from './app_parental_controls/mojo_interface_provider.js';
import {ParentalControlsDialogType, recordParentalControlsDialogFlowCompleted, recordParentalControlsDialogOpened} from './app_parental_controls/metrics_utils.js';
import {getTemplate} from './os_apps_page.html.js';

export function isAppInstalled(app: AppWithNotifications): boolean {
  switch (app.readiness) {
    case Readiness.kReady:
    case Readiness.kDisabledByBlocklist:
    case Readiness.kDisabledByPolicy:
    case Readiness.kDisabledByUser:
    case Readiness.kTerminated:
    case Readiness.kDisabledByLocalSettings:
      return true;
    case Readiness.kUninstalledByUser:
    case Readiness.kUninstalledByNonUser:
    case Readiness.kRemoved:
    case Readiness.kUnknown:
      return false;
  }
}

const OsSettingsAppsPageElementBase = DeepLinkingMixin(RouteOriginMixin(
    PrefsMixin(AppManagementStoreMixin(I18nMixin(PolymerElement)))));

export class OsSettingsAppsPageElement extends OsSettingsAppsPageElementBase {
  static get is() {
    return 'os-settings-apps-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      section_: {
        type: Number,
        value: Section.kApps,
        readOnly: true,
      },

      /**
       * This object holds the playStoreEnabled and settingsAppAvailable
       * boolean.
       */
      androidAppsInfo: Object,

      isPlayStoreAvailable_: {
        type: Boolean,
        value: () => {
          return isPlayStoreAvailable();
        },
      },

      searchTerm: String,

      showAndroidApps_: {
        type: Boolean,
        value: () => {
          return androidAppsVisible();
        },
      },

      isArcVmManageUsbAvailable_: {
        type: Boolean,
        value: () => {
          return isArcVmEnabled();
        },
      },

      /**
       * Whether the App Notifications page should be shown.
       */
      showAppNotificationsRow_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showOsSettingsAppNotificationsRow');
        },
      },

      /**
       * Whether the Manage Isolated Web Apps page should be shown.
       */
      showManageIsolatedWebAppsRow_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showManageIsolatedWebAppsRow');
        },
      },

      /**
       * Whether the Disable Parental Controls PIN dialog should be shown.
       */
      showParentalControlsDisablePinDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the Parental Controls PIN setup dialog should be shown.
       */
      showParentalControlsSetupPinDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the Parental Controls PIN verification dialog should be shown.
       */
      showParentalControlsVerifyPinDialog_: {
        type: Boolean,
        value: false,
      },

      /** Whether the user has set up app parental controls. */
      isParentalControlsSetupCompleted_: {
        type: Boolean,
        value: false,
      },

      isPluginVmAvailable_: {
        type: Boolean,
        value: () => {
          return isPluginVmAvailable();
        },
      },

      isAppParentalControlsFeatureAvailable_: {
        type: Boolean,
        value: () => {
          return isAppParentalControlsFeatureAvailable();
        },
        readOnly: true,
      },

      /**
       * Show On startup settings and sub-page.
       */
      shouldShowStartup_: {
        type: Boolean,
        value: () => {
          return shouldShowStartup();
        },
        readOnly: true,
      },

      app_: Object,

      appsWithNotifications_: {
        type: Array,
        value: [],
      },

      /**
       * List of options for the on startup drop-down menu.
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

      isDndEnabled_: {
        type: Boolean,
        value: false,
      },

      isPinVerified_: {
        type: Boolean,
        value: false,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kManageAndroidPreferences,
          Setting.kTurnOnPlayStore,
          Setting.kRestoreAppsAndPages,
          Setting.kAppParentalControls,
        ]),
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value() {
          return isRevampWayfindingEnabled();
        },
        readOnly: true,
      },

      rowIcons_: {
        type: Object,
        value() {
          if (isRevampWayfindingEnabled()) {
            return {
              manageApps: 'os-settings:apps',
              notifications: 'os-settings:apps-notifications',
              googlePlayPreferences: 'os-settings:google-play-revamp',
              androidSettings: 'os-settings:apps-android-settings',
              manageIsolatedWebApps:
                  'os-settings:apps-manage-isolated-web-apps',
              parentalControls: 'os-settings:apps-parental-controls',
            };
          }

          return {
            manageApps: '',
            notifications: '',
            googlePlayPreferences: '',
            androidSettings: '',
            manageIsolatedWebApps: '',
            parentalControls: '',
          };
        },
      },
    };
  }

  androidAppsInfo: AndroidAppsInfo;
  searchTerm: string;
  private app_: App;
  private appNotificationsObserverReceiver_: AppNotificationsObserverReceiver;
  private appsWithNotifications_: AppWithNotifications[];
  private isArcVmManageUsbAvailable_: boolean;
  private isDndEnabled_: boolean;
  private isPinVerified_: boolean;
  private readonly isPlayStoreAvailable_: boolean;
  private isPluginVmAvailable_: boolean;
  private isRevampWayfindingEnabled_: boolean;
  private mojoInterfaceProvider_: AppNotificationsHandlerInterface;
  private parentalControlsHandler_: AppParentalControlsHandlerInterface;
  private onStartupOptions_: DropdownMenuOptionList;
  private rowIcons_: Record<string, string>;
  private section_: Section;
  private readonly showAndroidApps_: boolean;
  private showAppNotificationsRow_: boolean;
  private showManageIsolatedWebAppsRow_: boolean;
  private showParentalControlsDisablePinDialog_: boolean;
  private showParentalControlsSetupPinDialog_: boolean;
  private showParentalControlsVerifyPinDialog_: boolean;
  private isParentalControlsSetupCompleted_: boolean;
  private readonly shouldShowStartup_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.APPS;
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.watch('app_', state => {
      // Don't set `app_` to `null`, since it triggers Polymer
      // data bindings of <app-management-uninstall-button> which does not
      // accept `null`, use `undefined` instead.
      return getSelectedApp(state) || undefined;
    });

    this.mojoInterfaceProvider_ = getAppNotificationProvider();

    this.appNotificationsObserverReceiver_ =
        new AppNotificationsObserverReceiver(this);

    this.mojoInterfaceProvider_.addObserver(
        this.appNotificationsObserverReceiver_.$.bindNewPipeAndPassRemote());

    this.mojoInterfaceProvider_.getQuietMode().then((result) => {
      this.isDndEnabled_ = result.enabled;
    });
    this.mojoInterfaceProvider_.getApps().then((result) => {
      this.appsWithNotifications_ = result.apps;
    });

    this.parentalControlsHandler_ = getAppParentalControlsProvider();
    this.getIsParentalControlsSetupCompleted_().then((isCompleted) => {
      this.isParentalControlsSetupCompleted_ = isCompleted;
    });
  }

  override ready(): void {
    super.ready();

    this.addFocusConfig(routes.APP_MANAGEMENT, '#appManagementRow');
    this.addFocusConfig(routes.APP_NOTIFICATIONS, '#appNotificationsRow');
    this.addFocusConfig(
        routes.MANAGE_ISOLATED_WEB_APPS, '#manageIsolatedWebAppsRow');
    this.addFocusConfig(
        routes.ANDROID_APPS_DETAILS,
        () => this.shadowRoot!.querySelector<HTMLElement>(
            this.androidAppsInfo.playStoreEnabled ?
                '#androidApps .subpage-arrow' :
                '#arcEnable'));
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    super.currentRouteChanged(newRoute, oldRoute);

    // Does not apply to this page.
    if (newRoute !== this.route) {
      return;
    }

    this.attemptDeepLink();
  }

  private iconUrlFromId_(app: App): string {
    if (!app) {
      return '';
    }
    return getAppIcon(app);
  }

  private onClickAppManagement_(): void {
    chrome.metricsPrivate.recordEnumerationValue(
        AppManagementEntryPointsHistogramName,
        AppManagementEntryPoint.OS_SETTINGS_MAIN_PAGE,
        Object.keys(AppManagementEntryPoint).length);
    Router.getInstance().navigateTo(routes.APP_MANAGEMENT);
  }

  private onClickAppNotifications_(): void {
    Router.getInstance().navigateTo(routes.APP_NOTIFICATIONS);
  }

  private async getIsParentalControlsSetupCompleted_(): Promise<boolean> {
    const response = await this.parentalControlsHandler_.isSetupCompleted();
    return response.isCompleted;
  }

  private onClickParentalControls_(): void {
    this.getIsParentalControlsSetupCompleted_().then((isSetupCompleted) => {
      if (isSetupCompleted) {
        this.showParentalControlsVerifyPinDialog_ = true;
        recordParentalControlsDialogOpened(
            ParentalControlsDialogType.ENTER_SUBPAGE_VERIFICATION);
      }
    });
  }

  private setUpParentalControls_(e: Event): void {
    this.showParentalControlsSetupPinDialog_ = true;
    recordParentalControlsDialogOpened(
        ParentalControlsDialogType.SET_UP_CONTROLS);
    // Stop propagation to keep the subpage from opening.
    e.stopPropagation();
  }

  private disableParentalControls_(e: Event): void {
    this.showParentalControlsDisablePinDialog_ = true;
    recordParentalControlsDialogOpened(
        ParentalControlsDialogType.DISABLE_CONTROLS_VERIFICATION);
    // Stop propagation to keep the subpage from opening.
    e.stopPropagation();
  }

  private onAccessPinVerified_(): void {
    this.navigateToParentalControls_();
    recordParentalControlsDialogFlowCompleted(
        ParentalControlsDialogType.ENTER_SUBPAGE_VERIFICATION);
  }

  private onSetupPinSuccess_(): void {
    this.navigateToParentalControls_();

    this.getIsParentalControlsSetupCompleted_().then((isCompleted) => {
      this.isParentalControlsSetupCompleted_ = isCompleted;
    });
    recordParentalControlsDialogFlowCompleted(
        ParentalControlsDialogType.SET_UP_CONTROLS);
  }

  private onDisablePinVerified_(): void {
    this.parentalControlsHandler_.onControlsDisabled();
    this.getIsParentalControlsSetupCompleted_().then((isCompleted) => {
      this.isParentalControlsSetupCompleted_ = isCompleted;
    });
    recordParentalControlsDialogFlowCompleted(
        ParentalControlsDialogType.DISABLE_CONTROLS_VERIFICATION);
  }

  private onVerifyPinDialogClose_(): void {
    this.showParentalControlsVerifyPinDialog_ = false;
  }

  private onSetupPinDialogClose_(): void {
    this.showParentalControlsSetupPinDialog_ = false;
  }

  private async onDisablePinDialogClose_(): Promise<void> {
    this.showParentalControlsDisablePinDialog_ = false;
    const toggle =
        this.shadowRoot!.querySelector<HTMLElement>('#appParentalControls')!
            .querySelector<CrToggleElement>('#toggle');
    // If the toggle is still on the page, reset toggle in case the disable flow
    // was cancelled prior to completion.
    if (toggle) {
      toggle.checked = await this.getIsParentalControlsSetupCompleted_();
    }
  }

  private onClickManageIsolatedWebApps_(): void {
    Router.getInstance().navigateTo(routes.MANAGE_ISOLATED_WEB_APPS);
  }

  private onEnableAndroidAppsClick_(event: Event): void {
    this.setPrefValue('arc.enabled', true);
    event.stopPropagation();
  }

  private isEnforced_(pref: chrome.settingsPrivate.PrefObject): boolean {
    return pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  }

  private onAndroidAppsSubpageClick_(): void {
    if (this.androidAppsInfo.playStoreEnabled) {
      Router.getInstance().navigateTo(routes.ANDROID_APPS_DETAILS);
    }
  }

  private onManageAndroidAppsClick_(event: MouseEvent): void {
    // |event.detail| is the click count. Keyboard events will have 0 clicks.
    const isKeyboardAction = event.detail === 0;
    AndroidAppsBrowserProxyImpl.getInstance().showAndroidAppsSettings(
        isKeyboardAction);
  }

  /** Override ash.settings.appNotification.onNotificationAppChanged */
  onNotificationAppChanged(updatedApp: AppWithNotifications): void {
    const foundIdx = this.appsWithNotifications_.findIndex(app => {
      return app.id === updatedApp.id;
    });
    if (isAppInstalled(updatedApp)) {
      if (foundIdx !== -1) {
        this.splice('appsWithNotifications_', foundIdx, 1, updatedApp);
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
  onQuietModeChanged(enabled: boolean): void {
    this.isDndEnabled_ = enabled;
  }

  private getAppNotificationsRowSublabel_(): string {
    if (this.isRevampWayfindingEnabled_) {
      return this.i18n('appNotificationsRowSublabel');
    }

    return this.isDndEnabled_ ?
        this.i18n('appNotificationsDoNotDisturbEnabledDescription') :
        this.i18n(
            'appNotificationsCountDescription',
            this.appsWithNotifications_.length);
  }

  private navigateToParentalControls_(): void {
    this.isPinVerified_ = true;
    Router.getInstance().navigateTo(routes.APP_PARENTAL_CONTROLS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsAppsPageElement.is]: OsSettingsAppsPageElement;
  }
}

customElements.define(OsSettingsAppsPageElement.is, OsSettingsAppsPageElement);
