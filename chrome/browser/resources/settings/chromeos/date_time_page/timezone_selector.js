// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'timezone-selector' is the time zone selector dropdown.
 */

Polymer({
  is: 'timezone-selector',

  behaviors: [PrefsBehavior],

  properties: {
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
    },

    /**
     * Initialized with the current time zone so the menu displays the
     * correct value. The full option list is fetched lazily if necessary by
     * maybeGetTimeZoneList_.
     * @private {!DropdownMenuOptionList}
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
  },

  observers: [
    'maybeGetTimeZoneListPerUser_(' +
        'prefs.settings.timezone.value,' +
        'prefs.generated.resolve_timezone_by_geolocation_on_off.value)',
    'maybeGetTimeZoneListPerSystem_(' +
        'prefs.cros.system.timezone.value,' +
        'prefs.generated.resolve_timezone_by_geolocation_on_off.value)',
    'updateActiveTimeZoneName_(prefs.cros.system.timezone.value)',
  ],

  /** @override */
  attached() {
    this.maybeGetTimeZoneList_();
  },

  /**
   * True if getTimeZones request was sent to Chrome, but result is not
   * yet received.
   */
  getTimeZonesRequestSent_: false,

  /**
   * Fetches the list of time zones if necessary.
   * @param {boolean=} perUserTimeZoneMode Expected value of per-user time zone.
   * @private
   * @suppress {missingProperties} Property finally never defined on
   */
  maybeGetTimeZoneList_(perUserTimeZoneMode) {
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
    settings.TimeZoneBrowserProxyImpl.getInstance()
        .getTimeZones()
        .then(timezones => {
          this.setTimeZoneList_(timezones);
        })
        .finally(() => {
          this.getTimeZonesRequestSent_ = false;
        });
  },

  /**
   * Prefs observer for Per-user time zone enabled mode.
   * @private
   */
  maybeGetTimeZoneListPerUser_() {
    this.maybeGetTimeZoneList_(true);
  },

  /**
   * Prefs observer for Per-user time zone disabled mode.
   * @private
   */
  maybeGetTimeZoneListPerSystem_() {
    this.maybeGetTimeZoneList_(false);
  },

  /**
   * Converts the C++ response into an array of menu options.
   * @param {!Array<!Array<string>>} timeZones C++ time zones response.
   * @private
   */
  setTimeZoneList_(timeZones) {
    this.timeZoneList_ = timeZones.map(function(timeZonePair) {
      return {
        name: timeZonePair[1],
        value: timeZonePair[0],
      };
    });
    this.updateActiveTimeZoneName_(
        /** @type {!String} */ (this.getPref('cros.system.timezone').value));
  },

  /**
   * Updates active time zone display name when changed.
   * @param {!String} activeTimeZoneId value of cros.system.timezone preference.
   * @private
   */
  updateActiveTimeZoneName_(activeTimeZoneId) {
    const activeTimeZone = this.timeZoneList_.find(
        (timeZone) => timeZone.value.toString() === activeTimeZoneId);
    if (activeTimeZone) {
      this.activeTimeZoneDisplayName = activeTimeZone.name;
    }
  },


  /**
   * Computes visibility of user timezone preference.
   * @param {?chrome.settingsPrivate.PrefObject} prefUserTimezone
   *     pref.settings.timezone
   * @param {boolean} prefResolveOnOffValue
   *     prefs.generated.resolve_timezone_by_geolocation_on_off.value
   * @return {boolean}
   * @private
   */
  isUserTimeZoneSelectorHidden_(prefUserTimezone, prefResolveOnOffValue) {
    return (prefUserTimezone && prefUserTimezone.controlledBy != null) ||
        prefResolveOnOffValue;
  },
});
