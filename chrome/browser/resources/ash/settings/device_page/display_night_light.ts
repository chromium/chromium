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
import '/shared/settings/controls/settings_slider.js';
import '/shared/settings/controls/settings_dropdown_menu.js';
import '/shared/settings/controls/settings_toggle_button.js';
import 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {GeolocationAccessLevel} from '../os_privacy_page/privacy_hub_geolocation_subpage.js';

import {getTemplate} from './display_night_light.html.js';

/**
 * The types of Night Light automatic schedule. The values of the enum values
 * are synced with the pref "prefs.ash.night_light.schedule_type".
 */
enum NightLightScheduleType {
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

      shouldShowEnableGeolocationDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [
      'updateNightLightScheduleSettings_(prefs.ash.night_light.schedule_type.*,' +
          ' prefs.ash.night_light.enabled.*),',
    ];
  }

  private nightLightScheduleSubLabel_: string;
  private scheduleTypesList_: ScheduleType[];
  private shouldOpenCustomScheduleCollapse_: boolean;
  private shouldShowGeolocationDialog_: boolean;
  private shouldShowGeolocationWarningText_: boolean;
  /**
   * Invoked when the status of Night Light or its schedule type are changed,
   * in order to update the schedule settings, such as whether to show the
   * custom schedule slider, and the schedule sub label.
   */
  private updateNightLightScheduleSettings_(): void {
    const scheduleType = this.getPref('ash.night_light.schedule_type').value;
    this.shouldOpenCustomScheduleCollapse_ =
        scheduleType === NightLightScheduleType.CUSTOM;

    if (scheduleType === NightLightScheduleType.SUNSET_TO_SUNRISE) {
      const nightLightStatus = this.getPref('ash.night_light.enabled').value;
      this.nightLightScheduleSubLabel_ = nightLightStatus ?
          this.i18n('displayNightLightOffAtSunrise') :
          this.i18n('displayNightLightOnAtSunset');
    } else {
      this.nightLightScheduleSubLabel_ = '';
    }
  }

  private computeShouldShowGeolocationWarningText_(): boolean {
    const scheduleType = this.prefs.ash.night_light.schedule_type.value;
    const geolocationAccessLevel =
        this.prefs.ash.user.geolocation_access_level.value;

    return (
        scheduleType === NightLightScheduleType.SUNSET_TO_SUNRISE &&
        geolocationAccessLevel === GeolocationAccessLevel.DISALLOWED);
  }

  private openGeolocationDialog_(): void {
    this.shouldShowGeolocationDialog_ = true;
  }

  private onGeolocationDialogClose_(): void {
    this.shouldShowGeolocationDialog_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsDisplayNightLightElement.is]: SettingsDisplayNightLightElement;
  }
}

customElements.define(
    SettingsDisplayNightLightElement.is, SettingsDisplayNightLightElement);
