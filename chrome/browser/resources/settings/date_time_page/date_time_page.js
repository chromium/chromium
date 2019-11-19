// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-date-time-page' is the settings page containing date and time
 * settings.
 */

Polymer({
  is: 'settings-date-time-page',

  behaviors: [I18nBehavior, PrefsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * Whether date and time are settable. Normally the date and time are forced
     * by network time, so default to false to initially hide the button.
     * @private
     */
    canSetDateTime_: {
      type: Boolean,
      value: false,
    },

    /**
     * This is used to get current time zone display name from
     * <timezone-selector> via bi-directional binding.
     */
    activeTimeZoneDisplayName: {
      type: String,
      value: loadTimeData.getString('timeZoneName'),
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.DATETIME_TIMEZONE_SUBPAGE) {
          map.set(
              settings.routes.DATETIME_TIMEZONE_SUBPAGE.path,
              '#timeZoneSettingsTrigger');
        }
        return map;
      },
    },

    /** @private */
    timeZoneSettingSubLabel_: {
      type: String,
      computed: `computeTimeZoneSettingSubLabel_(
          activeTimeZoneDisplayName,
          prefs.generated.resolve_timezone_by_geolocation_on_off.value,
          prefs.generated.resolve_timezone_by_geolocation_method_short.value)`
    },

    /** @private */
    isChild_: {type: Boolean, value: loadTimeData.getBoolean('isChild')},

    /**
     * Whether the icon informing that this action is managed by a parent is
     * displayed.
     * @private
     */
    displayManagedByParentIcon_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isChild') &&
          loadTimeData.getBoolean('timeActionsProtectedForChild')
    },
  },

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'can-set-date-time-changed', this.onCanSetDateTimeChanged_.bind(this));
    this.addWebUIListener(
        'access-code-validation-complete',
        this.openTimeZoneSubpage_.bind(this));

    chrome.send('dateTimePageReady');
  },

  /**
   * @param {boolean} canSetDateTime Whether date and time are settable.
   * @private
   */
  onCanSetDateTimeChanged_: function(canSetDateTime) {
    this.canSetDateTime_ = canSetDateTime;
  },

  /** @private */
  onSetDateTimeTap_: function() {
    chrome.send('showSetDateTimeUI');
  },

  /**
   * @return {string}
   * @private
   */
  computeTimeZoneSettingSubLabel_: function() {
    if (!this.getPref('generated.resolve_timezone_by_geolocation_on_off')
             .value) {
      return this.activeTimeZoneDisplayName;
    }
    const method = /** @type {number} */ (
        this.getPref('generated.resolve_timezone_by_geolocation_method_short')
            .value);
    const id = [
      'setTimeZoneAutomaticallyDisabled',
      'setTimeZoneAutomaticallyIpOnlyDefault',
      'setTimeZoneAutomaticallyWithWiFiAccessPointsData',
      'setTimeZoneAutomaticallyWithAllLocationInfo',
    ][method];
    return id ? this.i18n(id) : '';
  },

  /**
   * Called when the timezone row is clicked. Child accounts need parental
   * approval to modify their timezone, this method starts this process on the
   * C++ side, and once it is complete the 'access-code-validation-complete'
   * event is triggered which invokes openTimeZoneSubpage_. For non-child
   * accounts the method is invoked immediately.
   * @private
   */
  onTimeZoneSettings_: function() {
    if (this.isChild_) {
      chrome.send('handleShowParentAccessForTimeZone');
      return;
    }
    this.openTimeZoneSubpage_();
  },

  /** @private */
  openTimeZoneSubpage_: function() {
    settings.navigateTo(settings.routes.DATETIME_TIMEZONE_SUBPAGE);
  },
});
