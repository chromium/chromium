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

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    PrefsBehavior,
    settings.RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

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
      value() {
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

    /**
     * Whether the icon informing that this action is managed by a parent is
     * displayed.
     * @private
     */
    displayManagedByParentIcon_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isChild'),
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.k24HourClock,
        chromeos.settings.mojom.Setting.kChangeTimeZone,
      ]),
    },
  },

  /** @private {?settings.TimeZoneBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.TimeZoneBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'can-set-date-time-changed', this.onCanSetDateTimeChanged_.bind(this));
    this.browserProxy_.dateTimePageReady();
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.DATETIME) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @param {boolean} canSetDateTime Whether date and time are settable.
   * @private
   */
  onCanSetDateTimeChanged_(canSetDateTime) {
    this.canSetDateTime_ = canSetDateTime;
  },

  /** @private */
  onSetDateTimeTap_() {
    this.browserProxy_.showSetDateTimeUI();
  },

  /**
   * @return {string}
   * @private
   */
  computeTimeZoneSettingSubLabel_() {
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

  /** @private */
  onTimeZoneSettings_() {
    this.openTimeZoneSubpage_();
  },

  /** @private */
  openTimeZoneSubpage_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.DATETIME_TIMEZONE_SUBPAGE);
  },
});
