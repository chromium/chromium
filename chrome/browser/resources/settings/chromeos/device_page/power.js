// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-power' is the settings subpage for power settings.
 */

import '//resources/cr_elements/policy/cr_policy_indicator.m.js';
import '//resources/cr_elements/md_select_css.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared_css.js';

import {assertNotReached} from '//resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from '//resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {BatteryStatus, DevicePageBrowserProxy, DevicePageBrowserProxyImpl, IdleBehavior, LidClosedBehavior, PowerManagementSettings, PowerSource} from './device_page_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsPowerElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior, I18nBehavior, RouteObserverBehavior,
      WebUIListenerBehavior
    ],
    PolymerElement);

/** @polymer */
class SettingsPowerElement extends SettingsPowerElementBase {
  static get is() {
    return 'settings-power';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {string} ID of the selected power source, or ''. */
      selectedPowerSourceId_: String,

      /** @private {!BatteryStatus|undefined} */
      batteryStatus_: Object,

      /** @private {boolean} Whether a low-power (USB) charger is being used. */
      lowPowerCharger_: Boolean,

      /**
         @private {boolean} Whether the AC idle behavior is managed by policy.
           */
      acIdleManaged_: Boolean,

      /**
       * @private {boolean} Whether the battery idle behavior is managed by
       *     policy.
       */
      batteryIdleManaged_: Boolean,

      /**
         @private {string} Text for label describing the lid-closed behavior.
           */
      lidClosedLabel_: String,

      /** @private {boolean} Whether the system possesses a lid. */
      hasLid_: Boolean,

      /**
       * List of available dual-role power sources.
       * @private {!Array<!PowerSource>|undefined}
       */
      powerSources_: Array,

      /** @private */
      powerSourceLabel_: {
        type: String,
        computed:
            'computePowerSourceLabel_(powerSources_, batteryStatus_.calculating)',
      },

      /** @private */
      showPowerSourceDropdown_: {
        type: Boolean,
        computed: 'computeShowPowerSourceDropdown_(powerSources_)',
        value: false,
      },

      /**
       * The name of the dedicated charging device being used, if present.
       * @private {string}
       */
      powerSourceName_: {
        type: String,
        computed: 'computePowerSourceName_(powerSources_, lowPowerCharger_)',
      },

      /**
         @private {Array<!{value: IdleBehavior, name: string, selected:
             boolean}>}
       */
      acIdleOptions_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
         @private {Array<!{value: IdleBehavior, name: string, selected:
             boolean}>}
       */
      batteryIdleOptions_: {
        type: Array,
        value() {
          return [];
        },
      },

      /** @private {boolean} */
      shouldAcIdleSelectBeDisabled_: {
        type: Boolean,
        computed: 'hasSingleOption_(acIdleOptions_)',
      },

      /** @private {boolean} */
      shouldBatteryIdleSelectBeDisabled_: {
        type: Boolean,
        computed: 'hasSingleOption_(batteryIdleOptions_)',
      },

      /** @private {boolean} */
      adaptiveChargingEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isAdaptiveChargingEnabled');
        },
      },

      /** @private {!chrome.settingsPrivate.PrefObject} */
      lidClosedPref_: {
        type: Object,
        value() {
          return /** @type {!chrome.settingsPrivate.PrefObject} */ ({});
        },
      },

      /** @private {!chrome.settingsPrivate.PrefObject} */
      adaptiveChargingPref_: {
        type: Object,
        value() {
          return /** @type {!chrome.settingsPrivate.PrefObject} */ ({});
        },
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!chromeos.settings.mojom.Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          chromeos.settings.mojom.Setting.kPowerIdleBehaviorWhileCharging,
          chromeos.settings.mojom.Setting.kPowerSource,
          chromeos.settings.mojom.Setting.kSleepWhenLaptopLidClosed,
          chromeos.settings.mojom.Setting.kPowerIdleBehaviorWhileOnBattery,
          chromeos.settings.mojom.Setting.kAdaptiveCharging,
        ]),
      },

    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!DevicePageBrowserProxy} */
    this.browserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'battery-status-changed', this.set.bind(this, 'batteryStatus_'));
    this.addWebUIListener(
        'power-sources-changed', this.powerSourcesChanged_.bind(this));
    this.browserProxy_.updatePowerStatus();

    this.addWebUIListener(
        'power-management-settings-changed',
        this.powerManagementSettingsChanged_.bind(this));
    this.browserProxy_.requestPowerManagementSettings();
  }

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    if (settingId === chromeos.settings.mojom.Setting.kPowerSource &&
        this.$.powerSource.hidden) {
      // If there is only 1 power source, there is no dropdown to focus.
      // Stop the deep link attempt in this case.
      return false;
    }

    // Continue with deep link attempt.
    return true;
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.POWER) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * @param {!Array<!PowerSource>|undefined} powerSources
   * @param {boolean} calculating
   * @return {string} The primary label for the power source row.
   * @private
   */
  computePowerSourceLabel_(powerSources, calculating) {
    return this.i18n(
        calculating ?
            'calculatingPower' :
            powerSources && powerSources.length ? 'powerSourceLabel' :
                                                  'powerSourceBattery');
  }

  /**
   * @param {!Array<!PowerSource>} powerSources
   * @return {boolean} True if at least one power source is attached and all of
   *     them are dual-role (no dedicated chargers).
   * @private
   */
  computeShowPowerSourceDropdown_(powerSources) {
    return powerSources.length > 0 && powerSources.every(function(source) {
      return !source.is_dedicated_charger;
    });
  }

  /**
   * @param {!Array<!PowerSource>} powerSources
   * @param {boolean} lowPowerCharger
   * @return {string} Description of the power source.
   * @private
   */
  computePowerSourceName_(powerSources, lowPowerCharger) {
    if (lowPowerCharger) {
      return this.i18n('powerSourceLowPowerCharger');
    }
    if (powerSources.length) {
      return this.i18n('powerSourceAcAdapter');
    }
    return '';
  }

  /** @private */
  onPowerSourceChange_() {
    this.browserProxy_.setPowerSource(this.$.powerSource.value);
  }

  /**
   * Used to disable Battery/AC idle select dropdowns.
   * @param {!Array<string>} idleOptions
   * @return {boolean}
   * @private
   */
  hasSingleOption_(idleOptions) {
    return idleOptions.length === 1;
  }

  /**
   * @param {!Event} event
   * @private
   */
  onAcIdleSelectChange_(event) {
    const behavior = /** @type {IdleBehavior} */
        (parseInt(event.target.value, 10));
    this.browserProxy_.setIdleBehavior(behavior, true /* whenOnAc */);
    recordSettingChange();
  }

  /** @private */
  onBatteryIdleSelectChange_() {
    const behavior = /** @type {IdleBehavior} */
        (parseInt(
            this.shadowRoot.querySelector('#batteryIdleSelect').value, 10));
    this.browserProxy_.setIdleBehavior(behavior, false /* whenOnAc */);
    recordSettingChange();
  }

  /** @private */
  onLidClosedToggleChange_() {
    // Other behaviors are only displayed when the setting is controlled, in
    // which case the toggle can't be changed by the user.
    this.browserProxy_.setLidClosedBehavior(
        this.$.lidClosedToggle.checked ? LidClosedBehavior.SUSPEND :
                                         LidClosedBehavior.DO_NOTHING);
    recordSettingChange();
  }

  /** @private */
  onAdaptiveChargingToggleChange_() {
    const /** @type {boolean} */ enabled =
        this.$.adaptiveChargingToggle.checked;
    this.browserProxy_.setAdaptiveCharging(enabled);
    recordSettingChange(
        chromeos.settings.mojom.Setting.kAdaptiveCharging,
        /** @type {!chromeos.settings.mojom.SettingChangeValue} */ ({
          boolValue: enabled
        }));
  }

  /**
   * @param {!Array<PowerSource>} sources External power sources.
   * @param {string} selectedId The ID of the currently used power source.
   * @param {boolean} lowPowerCharger Whether the currently used power source
   *     is a low-powered USB charger.
   * @private
   */
  powerSourcesChanged_(sources, selectedId, lowPowerCharger) {
    this.powerSources_ = sources;
    this.selectedPowerSourceId_ = selectedId;
    this.lowPowerCharger_ = lowPowerCharger;
  }

  /**
   * @param {LidClosedBehavior} behavior Current behavior.
   * @param {boolean} isControlled Whether the underlying pref is controlled.
   * @private
   */
  updateLidClosedLabelAndPref_(behavior, isControlled) {
    const pref = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      // Most behaviors get a dedicated label and appear as checked.
      value: true,
    };

    switch (behavior) {
      case LidClosedBehavior.SUSPEND:
      case LidClosedBehavior.DO_NOTHING:
        // "Suspend" and "do nothing" share the "sleep" label and communicate
        // their state via the toggle state.
        this.lidClosedLabel_ = loadTimeData.getString('powerLidSleepLabel');
        pref.value = behavior === LidClosedBehavior.SUSPEND;
        break;
      case LidClosedBehavior.STOP_SESSION:
        this.lidClosedLabel_ = loadTimeData.getString('powerLidSignOutLabel');
        break;
      case LidClosedBehavior.SHUT_DOWN:
        this.lidClosedLabel_ = loadTimeData.getString('powerLidShutDownLabel');
        break;
    }

    if (isControlled) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
    }

    this.lidClosedPref_ = pref;
  }

  /**
   * @param {!IdleBehavior} idleBehavior
   * @param {!IdleBehavior} currIdleBehavior
   * @return {{value: IdleBehavior, name: string, selected:boolean }}
   *     Idle option object that maps to idleBehavior.
   * @private
   */
  getIdleOption_(idleBehavior, currIdleBehavior) {
    const selected = idleBehavior === currIdleBehavior;
    switch (idleBehavior) {
      case IdleBehavior.DISPLAY_OFF_SLEEP:
        return {
          value: idleBehavior,
          name: loadTimeData.getString('powerIdleDisplayOffSleep'),
          selected: selected
        };
      case IdleBehavior.DISPLAY_OFF:
        return {
          value: idleBehavior,
          name: loadTimeData.getString('powerIdleDisplayOff'),
          selected: selected
        };
      case IdleBehavior.DISPLAY_ON:
        return {
          value: idleBehavior,
          name: loadTimeData.getString('powerIdleDisplayOn'),
          selected: selected
        };
      case IdleBehavior.SHUT_DOWN:
        return {
          value: idleBehavior,
          name: loadTimeData.getString('powerIdleDisplayShutDown'),
          selected: selected
        };
      case IdleBehavior.STOP_SESSION:
        return {
          value: idleBehavior,
          name: loadTimeData.getString('powerIdleDisplayStopSession'),
          selected: selected
        };
      default:
        assertNotReached('Unknown IdleBehavior type');
    }
  }

  /**
   * @param {!Array<!IdleBehavior>} acIdleBehaviors
   * @param {!Array<!IdleBehavior>} batteryIdleBehaviors
   * @private
   */
  updateIdleOptions_(
      acIdleBehaviors, batteryIdleBehaviors, currAcIdleBehavior,
      currBatteryIdleBehavior) {
    this.acIdleOptions_ = acIdleBehaviors.map((idleBehavior) => {
      return this.getIdleOption_(idleBehavior, currAcIdleBehavior);
    });

    this.batteryIdleOptions_ = batteryIdleBehaviors.map((idleBehavior) => {
      return this.getIdleOption_(idleBehavior, currBatteryIdleBehavior);
    });
  }

  /**
   * @param {!PowerManagementSettings} powerManagementSettings Current
   *     power management settings.
   * @private
   */
  powerManagementSettingsChanged_(powerManagementSettings) {
    this.updateIdleOptions_(
        powerManagementSettings.possibleAcIdleBehaviors || [],
        powerManagementSettings.possibleBatteryIdleBehaviors || [],
        powerManagementSettings.currentAcIdleBehavior,
        powerManagementSettings.currentBatteryIdleBehavior);
    this.acIdleManaged_ = powerManagementSettings.acIdleManaged;
    this.batteryIdleManaged_ = powerManagementSettings.batteryIdleManaged;
    this.hasLid_ = powerManagementSettings.hasLid;
    this.updateLidClosedLabelAndPref_(
        powerManagementSettings.lidClosedBehavior,
        powerManagementSettings.lidClosedControlled);
    // Use an atomic assign to trigger UI change.
    this.adaptiveChargingPref_ = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: powerManagementSettings.adaptiveCharging,
    };
  }

  /**
   * Returns the row class for the given settings row
   * @param {boolean} batteryPresent if battery is present
   * @param {string} element the name of the row being queried
   * @return {string} the class for the given row
   * @private
   */
  getClassForRow_(batteryPresent, element) {
    let c = 'cr-row';

    switch (element) {
      case 'adaptiveCharging':
        if (!batteryPresent) {
          c += ' first';
        }
        break;
      case 'idle':
        if (!batteryPresent && !this.adaptiveChargingEnabled_) {
          c += ' first';
        }
        break;
    }

    return c;
  }

  /**
   * @param {*} lhs
   * @param {*} rhs
   * @return {boolean}
   * @private
   */
  isEqual_(lhs, rhs) {
    return lhs === rhs;
  }
}

customElements.define(SettingsPowerElement.is, SettingsPowerElement);
