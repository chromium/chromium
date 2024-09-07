// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-display-night-light' is the section on the display subpage
 * for display night light preferences' adjustments.
 */

import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_scheduler_slider/settings_scheduler_slider.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import '../controls/settings_slider.js';
import '../controls/settings_dropdown_menu.js';
import '../controls/settings_toggle_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {DisplaySettingsNightLightScheduleOption, DisplaySettingsProviderInterface, DisplaySettingsType} from '../mojom-webui/display_settings_provider.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {PrivacyHubBrowserProxy, PrivacyHubBrowserProxyImpl} from '../os_privacy_page/privacy_hub_browser_proxy.js';
import {GeolocationAccessLevel} from '../os_privacy_page/privacy_hub_geolocation_subpage.js';

import {getTemplate} from './display_night_light.html.js';
import {getDisplaySettingsProvider} from './display_settings_mojo_interface_provider.js';

/**
 * The types of Night Light automatic schedule. The values of the enum values
 * are synced with the pref "prefs.ash.night_light.schedule_type".
 */
export enum NightLightScheduleType {
  NEVER = 0,
  SUNSET_TO_SUNRISE = 1,
  CUSTOM = 2,
}

/**
 * Required member fields for events which select displays.
 */
interface ScheduleType {
  name: string;
  value: NightLightScheduleType;
}

const SettingsDisplayNightLightElementBase =
    DeepLinkingMixin(PrefsMixin(I18nMixin(PolymerElement)));

export class SettingsDisplayNightLightElement extends
    SettingsDisplayNightLightElementBase {
  static get is() {
    return 'settings-display-night-light' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      scheduleTypesList_: {
        type: Array,
        value() {
          return [
            {
              name: loadTimeData.getString('displayNightLightScheduleNever'),
              value: NightLightScheduleType.NEVER,
            },
            {
              name: loadTimeData.getString(
                  'displayNightLightScheduleSunsetToSunRise'),
              value: NightLightScheduleType.SUNSET_TO_SUNRISE,
            },
            {
              name: loadTimeData.getString('displayNightLightScheduleCustom'),
              value: NightLightScheduleType.CUSTOM,
            },
          ];
        },
      },

      shouldOpenCustomScheduleCollapse_: {
        type: Boolean,
        value: false,
      },

      nightLightScheduleSubLabel_: String,

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kNightLight,
          Setting.kNightLightColorTemperature,
        ]),
      },

      shouldShowGeolocationWarningText_: {
        type: Boolean,
        computed: 'computeShouldShowGeolocationWarningText_(' +
            'prefs.ash.night_light.schedule_type.value, ' +
            'prefs.ash.user.geolocation_access_level.value),',
      },

      sunriseTime_: {
        type: String,
        value() {
          return loadTimeData.getString(
              'privacyHubSystemServicesInitSunRiseTime');
        },
      },

      sunsetTime_: {
        type: String,
        value() {
          return loadTimeData.getString(
              'privacyHubSystemServicesInitSunSetTime');
        },
      },

      geolocationWarningText_: {
        type: String,
        computed: 'computeGeolocationWarningText_(' +
            'prefs.ash.user.geolocation_access_level.*,' +
            'sunriseTime_, sunsetTime_)',

      },

      shouldShowEnableGeolocationDialog_: {
        type: Boolean,
        value: false,
      },

      isInternalDisplay: Boolean,

      /**
       * Current status of night light setting.
       */
      currentNightLightStatus: Boolean,

      /**
       * Current selected night light schedule type.
       */
      currentScheduleType: NightLightScheduleType,
    };
  }

  static get observers() {
    return [
      'updateNightLightScheduleSettings_(prefs.ash.night_light.schedule_type.*,' +
          ' prefs.ash.night_light.enabled.*),',
      'onTimeZoneChanged_(prefs.cros.system.timezone.value)',
    ];
  }

  isInternalDisplay: boolean;
  private displaySettingsProvider: DisplaySettingsProviderInterface =
      getDisplaySettingsProvider();
  private nightLightScheduleSubLabel_: string;
  private scheduleTypesList_: ScheduleType[];
  private shouldOpenCustomScheduleCollapse_: boolean;
  private shouldShowGeolocationDialog_: boolean;
  private shouldShowGeolocationWarningText_: boolean;
  private currentNightLightStatus: boolean;
  private currentScheduleType: NightLightScheduleType;
  private sunriseTime_: string;
  private sunsetTime_: string;
  private privacyHubBrowserProxy_: PrivacyHubBrowserProxy;

  constructor() {
    super();
    this.privacyHubBrowserProxy_ = PrivacyHubBrowserProxyImpl.getInstance();
  }
  /**
   * Invoked when the status of Night Light or its schedule type are changed,
   * in order to update the schedule settings, such as whether to show the
   * custom schedule slider, and the schedule sub label.
   */
  private updateNightLightScheduleSettings_(): void {
    const scheduleType = this.getPref('ash.night_light.schedule_type').value;
    this.shouldOpenCustomScheduleCollapse_ =
        scheduleType === NightLightScheduleType.CUSTOM;

    const nightLightStatus: boolean =
        this.getPref('ash.night_light.enabled').value;
    if (scheduleType === NightLightScheduleType.SUNSET_TO_SUNRISE) {
      this.nightLightScheduleSubLabel_ = nightLightStatus ?
          this.i18n('displayNightLightOffAtSunrise') :
          this.i18n('displayNightLightOnAtSunset');
    } else {
      this.nightLightScheduleSubLabel_ = '';
    }

    // Records metrics when schedule type or night light status have changed. Do
    // not record when the page just loads and the current value is still
    // undefined.
    if (this.currentScheduleType !== scheduleType &&
        this.currentScheduleType !== undefined) {
      this.recordChangingNightLightSchedule(
          this.isInternalDisplay, scheduleType);
    }
    if (this.currentNightLightStatus !== nightLightStatus &&
        this.currentNightLightStatus !== undefined) {
      this.recordTogglingNightLightStatus(
          this.isInternalDisplay, nightLightStatus);
    }

    // Updates current schedule type and night light status.
    this.currentScheduleType = scheduleType;
    this.currentNightLightStatus = nightLightStatus;
  }

  // Records metrics when users change the night light schedule.
  private recordChangingNightLightSchedule(
      isInternalDisplay: boolean,
      nightLightSchedule: DisplaySettingsNightLightScheduleOption): void {
    this.displaySettingsProvider.recordChangingDisplaySettings(
        DisplaySettingsType.kNightLightSchedule, {
          isInternalDisplay,
          nightLightSchedule,
          displayId: null,
          orientation: null,
          nightLightStatus: null,
          mirrorModeStatus: null,
          unifiedModeStatus: null,
        });
  }

  // Records metrics when users toggle the night light status.
  private recordTogglingNightLightStatus(
      isInternalDisplay: boolean, nightLightStatus: boolean): void {
    this.displaySettingsProvider.recordChangingDisplaySettings(
        DisplaySettingsType.kNightLight, {
          isInternalDisplay,
          nightLightStatus,
          displayId: null,
          orientation: null,
          nightLightSchedule: null,
          mirrorModeStatus: null,
          unifiedModeStatus: null,
        });
  }

  private computeShouldShowGeolocationWarningText_(): boolean {
    const scheduleType = this.prefs.ash.night_light.schedule_type.value;
    const geolocationAccessLevel =
        this.prefs.ash.user.geolocation_access_level.value;

    return (
        scheduleType === NightLightScheduleType.SUNSET_TO_SUNRISE &&
        geolocationAccessLevel === GeolocationAccessLevel.DISALLOWED);
  }

  private computeGeolocationWarningText_(): string {
    if (!this.prefs) {
      return '';
    }

    if (this.prefs.ash.user.geolocation_access_level.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      return loadTimeData.getStringF(
          'displayNightLightGeolocationManagedWarningText', this.sunriseTime_,
          this.sunsetTime_);
    } else {
      return loadTimeData.getStringF(
          'displayNightLightGeolocationWarningText', this.sunriseTime_,
          this.sunsetTime_);
    }
  }

  private openGeolocationDialog_(): void {
    this.shouldShowGeolocationDialog_ = true;
  }

  private onGeolocationDialogClose_(): void {
    this.shouldShowGeolocationDialog_ = false;
  }

  private onTimeZoneChanged_(): void {
    this.privacyHubBrowserProxy_.getCurrentSunriseTime().then((time) => {
      this.sunriseTime_ = time;
    });
    this.privacyHubBrowserProxy_.getCurrentSunsetTime().then((time) => {
      this.sunsetTime_ = time;
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsDisplayNightLightElement.is]: SettingsDisplayNightLightElement;
  }
}

customElements.define(
    SettingsDisplayNightLightElement.is, SettingsDisplayNightLightElement);
