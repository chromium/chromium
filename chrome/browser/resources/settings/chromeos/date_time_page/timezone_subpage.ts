// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'timezone-subpage' is the collapsible section containing
 * time zone settings.
 */
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import '/shared/settings/controls/controlled_radio_button.js';
import '/shared/settings/controls/settings_dropdown_menu.js';
import '/shared/settings/controls/settings_radio_group.js';
import '../settings_shared.css.js';
import './timezone_selector.js';

import {SettingsDropdownMenuElement} from '/shared/settings/controls/settings_dropdown_menu.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route} from '../router.js';

import {TimeZoneAutoDetectMethod} from './date_time_types.js';
import {TimeZoneBrowserProxy, TimeZoneBrowserProxyImpl} from './timezone_browser_proxy.js';
import {TimezoneSelectorElement} from './timezone_selector.js';
import {getTemplate} from './timezone_subpage.html.js';

export interface TimezoneSubpageElement {
  $: {
    timezoneSelector: TimezoneSelectorElement,
    timeZoneResolveMethodDropdown: SettingsDropdownMenuElement,
  };
}

const TimezoneSubpageElementBase = DeepLinkingMixin(
    RouteObserverMixin(PrefsMixin(WebUiListenerMixin(PolymerElement))));

export class TimezoneSubpageElement extends TimezoneSubpageElementBase {
  static get is() {
    return 'timezone-subpage';
  }

  static get template() {
    return getTemplate();
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
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([Setting.kChangeTimeZone]),
      },

      showEnableSystemGeolocationDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private browserProxy_: TimeZoneBrowserProxy;
  private showEnableSystemGeolocationDialog_: boolean;

  constructor() {
    super();

    this.browserProxy_ = TimeZoneBrowserProxyImpl.getInstance();
  }

  /**
   * RouteObserverMixin
   * Called when the timezone subpage is hit. Child accounts need parental
   * approval to modify their timezone, this method starts this process on the
   * C++ side, and timezone setting will be disable. Once it is complete the
   * 'access-code-validation-complete' event is triggered which invokes
   * enableTimeZoneSetting_.
   */
  override currentRouteChanged(newRoute: Route, _oldRoute?: Route) {
    if (newRoute !== routes.DATETIME_TIMEZONE_SUBPAGE) {
      return;
    }

    // Check if should ask for parent access code.
    if (loadTimeData.getBoolean('isChild')) {
      this.disableTimeZoneSetting_();
      this.addWebUiListener(
          'access-code-validation-complete',
          this.enableTimeZoneSetting_.bind(this));
      this.browserProxy_.showParentAccessForTimeZone();
    }

    this.attemptDeepLink();
  }

  /**
   * Returns value list for timeZoneResolveMethodDropdown menu.
   */
  private getTimeZoneResolveMethodsList_() {
    const result: Array<{name: string, value: number}> = [];
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
   */
  private enableTimeZoneSetting_() {
    const radios = this.shadowRoot!.querySelectorAll('controlled-radio-button');
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
   */
  private disableTimeZoneSetting_() {
    this.$.timeZoneResolveMethodDropdown.disabled = true;
    this.$.timezoneSelector.shouldDisableTimeZoneGeoSelector = true;
    const radios = this.shadowRoot!.querySelectorAll('controlled-radio-button');
    for (const radio of radios) {
      radio.disabled = true;
    }
  }

  private onTimeZoneSelectionChanged_(): void {
    const geolocationAllowed =
        this.getPref('ash.user.geolocation_allowed').value;
    if (geolocationAllowed) {
      return;
    }

    let selectedTimezoneOption = null;
    const dropDown = this.$.timeZoneResolveMethodDropdown;
    if (dropDown.pref) {
      selectedTimezoneOption = dropDown.pref.value;
    }

    // Pop up geolocation dialog, when user wants to enable precise timezone,
    // but the system geolocation access is disabled.
    if (selectedTimezoneOption ===
            TimeZoneAutoDetectMethod.SEND_ALL_LOCATION_INFO ||
        selectedTimezoneOption ===
            TimeZoneAutoDetectMethod.SEND_WIFI_ACCESS_POINTS) {
      this.showEnableSystemGeolocationDialog_ = true;
    }
  }

  private onEnableSystemGeolocationDialogClosed_(): void {
    this.showEnableSystemGeolocationDialog_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'timezone-subpage': TimezoneSubpageElement;
  }
}

customElements.define(TimezoneSubpageElement.is, TimezoneSubpageElement);
