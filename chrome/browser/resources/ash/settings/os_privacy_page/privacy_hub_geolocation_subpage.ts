// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-hub-geolocation-subpage' contains a detailed overview about
 * the state of the system geolocation access.
 */

import './privacy_hub_app_permission_row.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PermissionType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {isPermissionEnabled} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExhaustive, castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isSecondaryUser} from '../common/load_time_booleans.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {DropdownMenuOptionList, SettingsDropdownMenuElement} from '../controls/settings_dropdown_menu.js';
import {App, AppPermissionsHandlerInterface, AppPermissionsObserverReceiver} from '../mojom-webui/app_permission_handler.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {getAppPermissionProvider} from './mojo_interface_provider.js';
import {PrivacyHubBrowserProxy, PrivacyHubBrowserProxyImpl} from './privacy_hub_browser_proxy.js';
import {getTemplate} from './privacy_hub_geolocation_subpage.html.js';
import {LOCATION_PERMISSION_CHANGE_FROM_SETTINGS_HISTOGRAM_NAME} from './privacy_hub_metrics_util.js';

/**
 * Geolocation access levels for the ChromeOS system.
 * This must be kept in sync with `GeolocationAccessLevel` in
 * ash/constants/geolocation_access_level.h
 */
export enum GeolocationAccessLevel {
  DISALLOWED = 0,
  ALLOWED = 1,
  ONLY_ALLOWED_FOR_SYSTEM = 2,
}

export enum ScheduleType {
  NONE = 0,
  SUNSET_TO_SUNRISE = 1,
  CUSTOM = 2,
}

export const GEOLOCATION_ACCESS_LEVEL_ENUM_SIZE =
    Object.keys(GeolocationAccessLevel).length;

/**
 * Whether the app has location permission defined.
 */
function hasLocationPermission(app: App): boolean {
  return app.permissions[PermissionType.kLocation] !== undefined;
}

export interface SettingsPrivacyHubGeolocationSubpage {
  $: {
    geolocationDropdown: SettingsDropdownMenuElement,
  };
}

const SettingsPrivacyHubGeolocationSubpageBase =
    RouteObserverMixin(DeepLinkingMixin(PrefsMixin(I18nMixin(PolymerElement))));

export class SettingsPrivacyHubGeolocationSubpage extends
    SettingsPrivacyHubGeolocationSubpageBase {
  static get is() {
    return 'settings-privacy-hub-geolocation-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      geolocationMapTargets_: {
        type: Object,
        value(this: SettingsPrivacyHubGeolocationSubpage) {
          return [
            {
              value: GeolocationAccessLevel.ALLOWED,
              name: this.i18n('geolocationAccessLevelAllowed'),
            },
            {
              value: GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM,
              name: this.i18n('geolocationAccessLevelOnlyAllowedForSystem'),
            },
            {
              value: GeolocationAccessLevel.DISALLOWED,
              name: this.i18n('geolocationAccessLevelDisallowed'),
            },
          ];
        },
      },
      /**
       * Show the right description text for the selected geolocation mode.
       */
      geolocationModeDescriptionText_: {
        type: TrustedHTML,
        computed: 'computeGeolocationModeDescriptionText_(' +
            'prefs.ash.user.geolocation_access_level.value)',
      },
      /**
       * Apps with location permission defined.
       */
      appList_: {
        type: Array,
        value: [],
      },
      automaticTimeZoneText_: {
        type: String,
        computed: 'computeAutomaticTimeZoneText_(' +
            'prefs.ash.user.geolocation_access_level.value,' +
            'prefs.generated.resolve_timezone_by_geolocation_on_off.value,' +
            'currentTimeZoneName_)',
      },
      isSecondaryUser_: {
        type: Boolean,
        value() {
          return isSecondaryUser();
        },
        readOnly: true,
      },
      isGeolocationAllowedForApps_: {
        type: Boolean,
        computed: 'computedIsGeolocationAllowedForApps_(' +
            'prefs.ash.user.geolocation_access_level.value)',
      },
      currentTimeZoneName_: {
        type: String,
        notify: true,
      },
      currentSunRiseTime_: {
        type: String,
        notify: true,
      },
      currentSunSetTime_: {
        type: String,
        notify: true,
      },
      sunsetScheduleText_: {
        type: String,
        computed: 'computeSunsetScheduleText_(' +
            'prefs.ash.user.geolocation_access_level.value,' +
            `prefs.ash.night_light.schedule_type.value,` +
            'currentSunRiseTime_, currentSunSetTime_)',
      },
      localWeatherText_: {
        type: String,
        computed: 'computeLocalWeatherText_(' +
            'prefs.ash.user.geolocation_access_level.value,' +
            'prefs.settings.ambient_mode.enabled.value)',
      },
      darkThemeText_: {
        type: String,
        computed: `computeDarkThemeText_(` +
            'prefs.ash.user.geolocation_access_level.value,' +
            'prefs.ash.dark_mode.schedule_type.value)',
      },
    };
  }

  static get observers() {
    return [
      'onTimeZoneChanged_(prefs.cros.system.timezone.value)',
    ];
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.PRIVACY_HUB_GEOLOCATION_ADVANCED) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Used by DeepLinkingMixin to focus this page's deep links.
   */
  override supportedSettingIds = new Set([
    Setting.kGeolocationAdvanced,
  ]);

  private geolocationMapTargets_: DropdownMenuOptionList;
  private geolocationModeDescriptionText_: string;
  private appList_: App[];
  private appPermissionsObserverReceiver_: AppPermissionsObserverReceiver|null;
  private isSecondaryUser_: boolean;
  private isGeolocationAllowedForApps_: boolean;
  private mojoInterfaceProvider_: AppPermissionsHandlerInterface;
  private browserProxy_: PrivacyHubBrowserProxy;
  private currentTimeZoneName_: string;
  private currentSunRiseTime_: string;
  private currentSunSetTime_: string;

  constructor() {
    super();

    this.mojoInterfaceProvider_ = getAppPermissionProvider();

    this.appPermissionsObserverReceiver_ = null;
    this.browserProxy_ = PrivacyHubBrowserProxyImpl.getInstance();
    // Assigning the initial time zone name.
    this.currentTimeZoneName_ = this.i18n('timeZoneName');
    this.currentSunRiseTime_ =
        this.i18n('privacyHubSystemServicesInitSunRiseTime');
    this.currentSunSetTime_ =
        this.i18n('privacyHubSystemServicesInitSunSetTime');
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.appPermissionsObserverReceiver_ =
        new AppPermissionsObserverReceiver(this);
    this.mojoInterfaceProvider_.addObserver(
        this.appPermissionsObserverReceiver_.$.bindNewPipeAndPassRemote());

    this.updateAppList_();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.appPermissionsObserverReceiver_!.$.close();
  }

  private settingControlledByPrimaryUserText_(): string {
    return this.i18n(
        'geolocationControlledByPrimaryUserText',
        loadTimeData.getString('primaryUserEmail'));
  }

  /**
   * The function is used for sorting app names alphabetically.
   */
  private alphabeticalSort_(first: App, second: App): number {
    return first.name.localeCompare(second.name);
  }

  private async updateAppList_(): Promise<void> {
    const apps = (await this.mojoInterfaceProvider_.getApps()).apps;
    this.appList_ = apps.filter(hasLocationPermission);
  }

  private isLocationPermissionEnabled_(app: App): boolean {
    const permission = castExists(app.permissions[PermissionType.kLocation]);
    return isPermissionEnabled(permission.value);
  }

  /** Implements AppPermissionsObserver.OnAppUpdated */
  onAppUpdated(updatedApp: App): void {
    if (!hasLocationPermission(updatedApp)) {
      return;
    }
    const idx = this.appList_.findIndex(app => app.id === updatedApp.id);
    if (idx === -1) {
      // New app installed.
      this.push('appList_', updatedApp);
    } else {
      // An already installed app is updated.
      this.splice('appList_', idx, 1, updatedApp);
    }
  }

  /** Implements AppPermissionsObserver.OnAppRemoved */
  onAppRemoved(appId: string): void {
    const idx = this.appList_.findIndex(app => app.id === appId);
    if (idx !== -1) {
      this.splice('appList_', idx, 1);
    }
  }

  private computedIsGeolocationAllowedForApps_(): boolean {
    const accessLevel: GeolocationAccessLevel =
        this.getPref<GeolocationAccessLevel>(
                'ash.user.geolocation_access_level')
            .value;
    switch (accessLevel) {
      case GeolocationAccessLevel.ALLOWED:
        return true;
      case GeolocationAccessLevel.DISALLOWED:
      case GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM:
        return false;
      default:
        assertExhaustive(accessLevel);
    }
  }

  private isAutomaticTimeZoneConfigured_(): boolean {
    return this.getPref('generated.resolve_timezone_by_geolocation_on_off')
        .value;
  }

  private computeAutomaticTimeZoneText_(): string {
    if (!this.prefs) {
      return '';
    }

    if (!this.isAutomaticTimeZoneConfigured_()) {
      return this.i18n('privacyHubSystemServicesGeolocationNotConfigured');
    }

    return this.geolocationAllowedForSystem_() ?
        this.i18n('privacyHubSystemServicesAllowedText') :
        this.i18n(
            'privacyHubSystemServicesAutomaticTimeZoneBlockedText',
            this.currentTimeZoneName_);
  }

  private isNightLightConfiguredToUseGeolocation_(): boolean {
    return this.getPref<ScheduleType>('ash.night_light.schedule_type').value ===
        ScheduleType.SUNSET_TO_SUNRISE;
  }

  private computeSunsetScheduleText_(): string {
    if (!this.prefs) {
      return '';
    }

    if (!this.isNightLightConfiguredToUseGeolocation_()) {
      return this.i18n('privacyHubSystemServicesGeolocationNotConfigured');
    }

    return this.geolocationAllowedForSystem_() ?
        this.i18n('privacyHubSystemServicesAllowedText') :
        this.i18n(
            'privacyHubSystemServicesSunsetScheduleBlockedText',
            this.currentSunRiseTime_, this.currentSunSetTime_);
  }

  private onManagePermissionsInChromeRowClick_(): void {
    this.mojoInterfaceProvider_.openBrowserPermissionSettings(
        PermissionType.kLocation);
  }

  private computeGeolocationModeDescriptionText_(): TrustedHTML {
    const accessLevel: GeolocationAccessLevel =
        this.getPref<GeolocationAccessLevel>(
                'ash.user.geolocation_access_level')
            .value;
    switch (accessLevel) {
      case GeolocationAccessLevel.ALLOWED:
        return this.i18nAdvanced('geolocationAllowedModeDescription');
      case GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM:
        return this.i18nAdvanced(
            'geolocationOnlyAllowedForSystemModeDescription');
      case GeolocationAccessLevel.DISALLOWED:
        return this.i18nAdvanced('geolocationBlockedModeDescription');
      default:
        assertExhaustive(accessLevel);
    }
  }

  private recordMetric_(): void {
    const accessLevel = this.$.geolocationDropdown.pref!.value;

    chrome.metricsPrivate.recordEnumerationValue(
        LOCATION_PERMISSION_CHANGE_FROM_SETTINGS_HISTOGRAM_NAME, accessLevel,
        GEOLOCATION_ACCESS_LEVEL_ENUM_SIZE);
  }

  private geolocationAllowedForSystem_(): boolean {
    if (!this.prefs) {
      // Won't show blocked services and apps in case that the geolocation pref
      // is not yet loaded.
      return true;
    }
    return this.getPref<GeolocationAccessLevel>(
                   'ash.user.geolocation_access_level')
               .value !== GeolocationAccessLevel.DISALLOWED;
  }

  private isLocalWeatherConfiguredToUseGeolocation_(): boolean {
    return this.getPref('settings.ambient_mode.enabled').value;
  }

  private computeLocalWeatherText_(): string {
    if (!this.prefs) {
      return '';
    }

    if (!this.isLocalWeatherConfiguredToUseGeolocation_()) {
      return this.i18n('privacyHubSystemServicesGeolocationNotConfigured');
    }

    return this.geolocationAllowedForSystem_() ?
        this.i18n('privacyHubSystemServicesAllowedText') :
        this.i18n('privacyHubSystemServicesBlockedText');
  }

  private isDarkThemeConfiguredToUseGeolocation_(): boolean {
    return this.getPref<ScheduleType>('ash.dark_mode.schedule_type').value ===
        ScheduleType.SUNSET_TO_SUNRISE;
  }

  private computeDarkThemeText_(): string {
    if (!this.prefs) {
      return '';
    }

    if (!this.isDarkThemeConfiguredToUseGeolocation_()) {
      return this.i18n('privacyHubSystemServicesGeolocationNotConfigured');
    }

    return this.geolocationAllowedForSystem_() ?
        this.i18n('privacyHubSystemServicesAllowedText') :
        this.i18n('privacyHubSystemServicesBlockedText');
  }
  private onTimeZoneChanged_(): void {
    this.browserProxy_.getCurrentTimeZoneName().then((timeZoneName) => {
      this.currentTimeZoneName_ = timeZoneName;
    });
    this.browserProxy_.getCurrentSunriseTime().then((time) => {
      this.currentSunRiseTime_ = time;
    });
    this.browserProxy_.getCurrentSunsetTime().then((time) => {
      this.currentSunSetTime_ = time;
    });
  }

  private onGeolocationAdvancedAreaClick_(): void {
    Router.getInstance().navigateTo(routes.PRIVACY_HUB_GEOLOCATION_ADVANCED);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPrivacyHubGeolocationSubpage.is]:
        SettingsPrivacyHubGeolocationSubpage;
  }
}

customElements.define(
    SettingsPrivacyHubGeolocationSubpage.is,
    SettingsPrivacyHubGeolocationSubpage);
