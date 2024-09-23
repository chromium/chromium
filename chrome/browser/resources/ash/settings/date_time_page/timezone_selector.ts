// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'timezone-selector' is the time zone selector dropdown.
 */

import '../settings_shared.css.js';
import '../controls/settings_dropdown_menu.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrSettingsPrefs} from '/shared/settings/prefs/prefs_types.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';

import {DateTimeBrowserProxy, DateTimePageHandlerRemote} from './date_time_browser_proxy.js';
import {getTemplate} from './timezone_selector.html.js';

const TimezoneSelectorElementBase = PrefsMixin(PolymerElement);

export class TimezoneSelectorElement extends TimezoneSelectorElementBase {
  static get is() {
    return 'timezone-selector';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * This stores active time zone display name to be used in other UI
       * via bi-directional binding.
       */
      activeTimeZoneDisplayName: {
        type: String,
        notify: true,
      },

      /**
       * True if the account is supervised and doesn't get parent access code
       * verification.
       */
      shouldDisableTimeZoneGeoSelector: {
        type: Boolean,
        notify: true,
        value: false,
      },

      /**
       * Initialized with the current time zone so the menu displays the
       * correct value. The full option list is fetched lazily if necessary by
       * maybeGetTimeZoneList_.
       */
      timeZoneList_: {
        type: Array,
        value() {
          return [{
            name: loadTimeData.getString('timeZoneName'),
            value: loadTimeData.getString('timeZoneID'),
          }];
        },
      },
    };
  }

  static get observers() {
    return [
      'maybeGetTimeZoneListPerUser_(' +
          'prefs.settings.timezone.value,' +
          'prefs.generated.resolve_timezone_by_geolocation_on_off.value)',
      'maybeGetTimeZoneListPerSystem_(' +
          'prefs.cros.system.timezone.value,' +
          'prefs.generated.resolve_timezone_by_geolocation_on_off.value)',
      'updateActiveTimeZoneName_(prefs.cros.system.timezone.value)',
    ];
  }

  activeTimeZoneDisplayName: string;
  shouldDisableTimeZoneGeoSelector: boolean;
  private timeZoneList_: DropdownMenuOptionList;
  private getTimeZonesRequestSent_: boolean;

  /**
   * Returns the browser proxy page handler (to invoke functions).
   */
  get pageHandler(): DateTimePageHandlerRemote {
    return DateTimeBrowserProxy.getInstance().handler;
  }

  constructor() {
    super();

    /**
     * True if getTimeZones request was sent to Chrome, but result is not
     * yet received.
     */
    this.getTimeZonesRequestSent_ = false;
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.maybeGetTimeZoneList_();
  }

  /**
   * Fetches the list of time zones if necessary.
   * @param perUserTimeZoneMode Expected value of per-user time zone.
   */
  private async maybeGetTimeZoneList_(perUserTimeZoneMode?: boolean):
      Promise<void> {
    if (typeof (perUserTimeZoneMode) !== 'undefined') {
      /* This method is called as observer. Skip if if current mode does not
       * match expected.
       */
      if (perUserTimeZoneMode !==
          this.getPref('cros.flags.per_user_timezone_enabled').value) {
        return;
      }
    }

    // Only fetch the list once.
    if (this.timeZoneList_.length > 1 || !CrSettingsPrefs.isInitialized) {
      return;
    }

    if (this.getTimeZonesRequestSent_) {
      return;
    }

    // If auto-detect is enabled, we only need the current time zone.
    if (this.getPref('generated.resolve_timezone_by_geolocation_on_off')
            .value) {
      const isPerUserTimezone =
          this.getPref('cros.flags.per_user_timezone_enabled').value;
      if (this.timeZoneList_[0].value ===
          (isPerUserTimezone ? this.getPref('settings.timezone').value :
                               this.getPref('cros.system.timezone').value)) {
        return;
      }
    }
    // Setting several preferences at once will trigger several
    // |maybeGetTimeZoneList_| calls, which we don't want.
    this.getTimeZonesRequestSent_ = true;
    try {
      const {timezones} = await this.pageHandler.getTimezones();
      this.setTimeZoneList_(timezones);
    } finally {
      this.getTimeZonesRequestSent_ = false;
    }
  }

  /**
   * Prefs observer for Per-user time zone enabled mode.
   */
  private maybeGetTimeZoneListPerUser_(): void {
    this.maybeGetTimeZoneList_(true);
  }

  /**
   * Prefs observer for Per-user time zone disabled mode.
   */
  private maybeGetTimeZoneListPerSystem_(): void {
    this.maybeGetTimeZoneList_(false);
  }

  /**
   * Converts the C++ response into an array of menu options.
   * @param timeZones C++ time zones response.
   */
  private setTimeZoneList_(timeZones: string[][]): void {
    this.timeZoneList_ = timeZones.map((timeZonePair) => {
      return {
        name: timeZonePair[1],
        value: timeZonePair[0],
      };
    });
    this.updateActiveTimeZoneName_(
        this.getPref<string>('cros.system.timezone').value);
  }

  /**
   * Updates active time zone display name when changed.
   * @param activeTimeZoneId value of cros.system.timezone preference.
   */
  private updateActiveTimeZoneName_(activeTimeZoneId: string): void {
    const activeTimeZone = this.timeZoneList_.find(
        (timeZone) => timeZone.value.toString() === activeTimeZoneId);
    if (activeTimeZone) {
      this.activeTimeZoneDisplayName = activeTimeZone.name;
    }
  }

  /**
   * Computes whether user timezone selector should be disabled. Returns `true`
   * if auto detect is on or it's waiting for 'access-code-validation-complete'
   * for child account.
   */
  private shouldDisableUserTimezoneSelector_(): boolean {
    return this.getPref<boolean>(
                   'generated.resolve_timezone_by_geolocation_on_off')
               .value ||
        this.shouldDisableTimeZoneGeoSelector;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'timezone-selector': TimezoneSelectorElement;
  }
}

customElements.define(TimezoneSelectorElement.is, TimezoneSelectorElement);
