// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'timezone-subpage' is the collapsible section containing
 * time zone settings.
 */
import '../../controls/controlled_radio_button.js';
import '../../controls/settings_radio_group.js';
import '../../prefs/prefs.js';
import '../../settings_shared.css.js';
import './timezone_selector.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {TimeZoneAutoDetectMethod} from './date_time_types.js';
import {TimeZoneBrowserProxy, TimeZoneBrowserProxyImpl} from './timezone_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const TimezoneSubpageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      PrefsBehavior,
      RouteObserverBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/** @polymer */
class TimezoneSubpageElement extends TimezoneSubpageElementBase {
  static get is() {
    return 'timezone-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * This is <timezone-selector> parameter.
       */
      activeTimeZoneDisplayName: {
        type: String,
        notify: true,
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([Setting.kChangeTimeZone]),
      },
    };
  }


  /** @override */
  constructor() {
    super();

    /** @private {?TimeZoneBrowserProxy} */
    this.browserProxy_ = TimeZoneBrowserProxyImpl.getInstance();
  }

  /**
   * RouteObserverBehavior
   * Called when the timezone subpage is hit. Child accounts need parental
   * approval to modify their timezone, this method starts this process on the
   * C++ side, and timezone setting will be disable. Once it is complete the
   * 'access-code-validation-complete' event is triggered which invokes
   * enableTimeZoneSetting_.
   * @param {!Route} newRoute
   * @param {!Route=} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
    if (newRoute !== routes.DATETIME_TIMEZONE_SUBPAGE) {
      return;
    }

    // Check if should ask for parent access code.
    if (loadTimeData.getBoolean('isChild')) {
      this.disableTimeZoneSetting_();
      this.addWebUIListener(
          'access-code-validation-complete',
          this.enableTimeZoneSetting_.bind(this));
      this.browserProxy_.showParentAccessForTimeZone();
    }

    this.attemptDeepLink();
  }

  /**
   * Returns value list for timeZoneResolveMethodDropdown menu.
   * @private
   */
  getTimeZoneResolveMethodsList_() {
    const result = [];
    const pref =
        this.getPref('generated.resolve_timezone_by_geolocation_method_short');
    // Make sure current value is in the list, even if it is not
    // user-selectable.
    if (pref.value === TimeZoneAutoDetectMethod.DISABLED) {
      // If disabled by policy, show the 'Automatic timezone disabled' label.
      // Otherwise, just show the default string, since the control will be
      // disabled as well.
      const label = pref.controlledBy ?
          loadTimeData.getString('setTimeZoneAutomaticallyDisabled') :
          loadTimeData.getString('setTimeZoneAutomaticallyIpOnlyDefault');
      result.push({value: TimeZoneAutoDetectMethod.DISABLED, name: label});
    }
    result.push({
      value: TimeZoneAutoDetectMethod.IP_ONLY,
      name: loadTimeData.getString('setTimeZoneAutomaticallyIpOnlyDefault'),
    });

    if (pref.value === TimeZoneAutoDetectMethod.SEND_WIFI_ACCESS_POINTS) {
      result.push({
        value: TimeZoneAutoDetectMethod.SEND_WIFI_ACCESS_POINTS,
        name: loadTimeData.getString(
            'setTimeZoneAutomaticallyWithWiFiAccessPointsData'),
      });
    }
    result.push({
      value: TimeZoneAutoDetectMethod.SEND_ALL_LOCATION_INFO,
      name:
          loadTimeData.getString('setTimeZoneAutomaticallyWithAllLocationInfo'),
    });
    return result;
  }

  /**
   * Enables all dropdowns and radio buttons.
   * @private
   */
  enableTimeZoneSetting_() {
    const radios = this.root.querySelectorAll('controlled-radio-button');
    for (const radio of radios) {
      radio.disabled = false;
    }
    this.$.timezoneSelector.shouldDisableTimeZoneGeoSelector = false;
    const pref =
        this.getPref('generated.resolve_timezone_by_geolocation_method_short');
    if (pref.value !== TimeZoneAutoDetectMethod.DISABLED) {
      this.$.timeZoneResolveMethodDropdown.disabled = false;
    }
  }

  /**
   * Disables all dropdowns and radio buttons.
   * @private
   */
  disableTimeZoneSetting_() {
    this.$.timeZoneResolveMethodDropdown.disabled = true;
    this.$.timezoneSelector.shouldDisableTimeZoneGeoSelector = true;
    const radios = this.root.querySelectorAll('controlled-radio-button');
    for (const radio of radios) {
      radio.disabled = true;
    }
  }
}

customElements.define(TimezoneSubpageElement.is, TimezoneSubpageElement);
