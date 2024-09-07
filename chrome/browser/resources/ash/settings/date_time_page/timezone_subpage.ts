// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'timezone-subpage' is the collapsible section containing
 * time zone settings.
 */
import '/shared/settings/prefs/prefs.js';
import '../controls/controlled_radio_button.js';
import '../controls/settings_dropdown_menu.js';
import '../controls/settings_radio_group.js';
import '../settings_shared.css.js';
import './timezone_selector.js';
import '../os_privacy_page/privacy_hub_geolocation_dialog.js';
import '../os_privacy_page/privacy_hub_geolocation_warning_text.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isChild} from '../common/load_time_booleans.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {SettingsDropdownMenuElement} from '../controls/settings_dropdown_menu.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {GeolocationAccessLevel} from '../os_privacy_page/privacy_hub_geolocation_subpage.js';
import {Route, routes} from '../router.js';

import {DateTimeBrowserProxy, DateTimePageCallbackRouter, DateTimePageHandlerRemote} from './date_time_browser_proxy.js';
import {TimeZoneAutoDetectMethod} from './date_time_types.js';
import {TimezoneSelectorElement} from './timezone_selector.js';
import {getTemplate} from './timezone_subpage.html.js';

export interface TimezoneSubpageElement {
  $: {
    timezoneSelector: TimezoneSelectorElement,
    timeZoneResolveMethodDropdown: SettingsDropdownMenuElement,
  };
}

const TimezoneSubpageElementBase =
    DeepLinkingMixin(RouteObserverMixin(I18nMixin(PrefsMixin(PolymerElement))));

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

      canSetSystemTimezone_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('canSetSystemTimezone');
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([Setting.kChangeTimeZone]),
      },

      geolocationWarningText_: {
        type: String,
        computed: 'computedGeolocationWarningText(activeTimeZoneDisplayName,' +
            'prefs.ash.user.geolocation_access_level.enforcement)',
      },

      shouldShowGeolocationWarningText_: {
        type: Boolean,
        computed: 'computeShouldShowGeolocationWarningText_(' +
            'prefs.generated.resolve_timezone_by_geolocation_on_off.value,' +
            'prefs.ash.user.geolocation_access_level.value)',
      },

      showEnableSystemGeolocationDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  activeTimeZoneDisplayName: string;
  private canSetSystemTimezone_: boolean;
  private browserProxy_: DateTimeBrowserProxy;
  private showEnableSystemGeolocationDialog_: boolean;
  private shouldShowGeolocationWarningText_: boolean;

  /**
   * Returns the browser proxy page handler (to invoke functions).
   */
  get pageHandler(): DateTimePageHandlerRemote {
    return this.browserProxy_.handler;
  }

  /**
   * Returns the browser proxy callback router (to receive async messages).
   */
  get callbackRouter(): DateTimePageCallbackRouter {
    return this.browserProxy_.observer;
  }

  constructor() {
    super();

    this.browserProxy_ = DateTimeBrowserProxy.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.callbackRouter.onParentAccessValidationComplete.addListener(
        this.enableTimeZoneSetting_.bind(this));
  }


  /**
   * RouteObserverMixin
   * Called when the timezone subpage is hit. Child accounts need parental
   * approval to modify their timezone, this method starts this process on the
   * C++ side, and timezone setting will be disable. Once it is complete the
   * 'access-code-validation-complete' event is triggered which invokes
   * enableTimeZoneSetting_.
   */
  override currentRouteChanged(newRoute: Route, _oldRoute?: Route): void {
    if (newRoute !== routes.DATETIME_TIMEZONE_SUBPAGE) {
      return;
    }

    // Check if should ask for parent access code.
    if (isChild()) {
      this.disableTimeZoneSetting_();
      this.pageHandler.showParentAccessForTimezone();
    }

    this.attemptDeepLink();
  }

  private computedGeolocationWarningText(): string {
    if (!this.prefs) {
      return '';
    }

    if (this.prefs.ash.user.geolocation_access_level.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      return loadTimeData.getStringF(
          'timeZoneGeolocationManagedWarningText',
          this.activeTimeZoneDisplayName);
    } else {
      return loadTimeData.getStringF(
          'timeZoneGeolocationWarningText', this.activeTimeZoneDisplayName);
    }
  }

  private computeShouldShowGeolocationWarningText_(): boolean {
    return (
        this.prefs.generated.resolve_timezone_by_geolocation_on_off.value ===
            true &&
        this.prefs.ash.user.geolocation_access_level.value ===
            GeolocationAccessLevel.DISALLOWED);
  }

  /**
   * Returns value list for timeZoneResolveMethodDropdown menu.
   */
  private getTimeZoneResolveMethodsList_():
      Array<{name: string, value: number}> {
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
  private enableTimeZoneSetting_(): void {
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
  private disableTimeZoneSetting_(): void {
    this.$.timeZoneResolveMethodDropdown.disabled = true;
    this.$.timezoneSelector.shouldDisableTimeZoneGeoSelector = true;
    const radios = this.shadowRoot!.querySelectorAll('controlled-radio-button');
    for (const radio of radios) {
      radio.disabled = true;
    }
  }

  private openGeolocationDialog_(): void {
    this.showEnableSystemGeolocationDialog_ = true;
  }

  private onGeolocationDialogClose_(): void {
    this.showEnableSystemGeolocationDialog_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'timezone-subpage': TimezoneSubpageElement;
  }
}

customElements.define(TimezoneSubpageElement.is, TimezoneSubpageElement);
