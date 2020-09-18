// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-power' is the settings subpage for power settings.
 */

Polymer({
  is: 'settings-power',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    settings.RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private {string} ID of the selected power source, or ''. */
    selectedPowerSourceId_: String,

    /** @private {!settings.BatteryStatus|undefined} */
    batteryStatus_: Object,

    /** @private {boolean} Whether a low-power (USB) charger is being used. */
    lowPowerCharger_: Boolean,

    /** @private {boolean} Whether the AC idle behavior is managed by policy. */
    acIdleManaged_: Boolean,

    /**
     * @private {boolean} Whether the battery idle behavior is managed by
     *     policy.
     */
    batteryIdleManaged_: Boolean,

    /** @private {string} Text for label describing the lid-closed behavior. */
    lidClosedLabel_: String,

    /** @private {boolean} Whether the system possesses a lid. */
    hasLid_: Boolean,

    /**
     * List of available dual-role power sources.
     * @private {!Array<!settings.PowerSource>|undefined}
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
       @private {Array<!{value: settings.IdleBehavior, name: string, selected:
           boolean}>}
     */
    acIdleOptions_: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
       @private {Array<!{value: settings.IdleBehavior, name: string, selected:
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

    /** @private {!chrome.settingsPrivate.PrefObject} */
    lidClosedPref_: {
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
      ]),
    },
  },

  /** @private {?settings.DevicePageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.DevicePageBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'battery-status-changed', this.set.bind(this, 'batteryStatus_'));
    this.addWebUIListener(
        'power-sources-changed', this.powerSourcesChanged_.bind(this));
    this.browserProxy_.updatePowerStatus();

    this.addWebUIListener(
        'power-management-settings-changed',
        this.powerManagementSettingsChanged_.bind(this));
    this.browserProxy_.requestPowerManagementSettings();
  },

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
  },

  /**
   * @param {!settings.Route} route
   * @param {settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.POWER) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @param {!Array<!settings.PowerSource>|undefined} powerSources
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
  },

  /**
   * @param {!Array<!settings.PowerSource>} powerSources
   * @return {boolean} True if at least one power source is attached and all of
   *     them are dual-role (no dedicated chargers).
   * @private
   */
  computeShowPowerSourceDropdown_(powerSources) {
    return powerSources.length > 0 && powerSources.every(function(source) {
      return !source.is_dedicated_charger;
    });
  },

  /**
   * @param {!Array<!settings.PowerSource>} powerSources
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
  },

  /** @private */
  onPowerSourceChange_() {
    this.browserProxy_.setPowerSource(this.$.powerSource.value);
  },

  /**
   * Used to disable Battery/AC idle select dropdowns.
   * @param {!Array<string>} idleOptions
   * @return {boolean}
   * @private
   */
  hasSingleOption_(idleOptions) {
    return idleOptions.length == 1;
  },

  /** @private */
  onAcIdleSelectChange_() {
    const behavior = /** @type {settings.IdleBehavior} */
        (parseInt(this.$$('#acIdleSelect').value, 10));
    this.browserProxy_.setIdleBehavior(behavior, true /* whenOnAc */);
    settings.recordSettingChange();
  },

  /** @private */
  onBatteryIdleSelectChange_() {
    const behavior = /** @type {settings.IdleBehavior} */
        (parseInt(this.$$('#batteryIdleSelect').value, 10));
    this.browserProxy_.setIdleBehavior(behavior, false /* whenOnAc */);
    settings.recordSettingChange();
  },

  /** @private */
  onLidClosedToggleChange_() {
    // Other behaviors are only displayed when the setting is controlled, in
    // which case the toggle can't be changed by the user.
    this.browserProxy_.setLidClosedBehavior(
        this.$.lidClosedToggle.checked ? settings.LidClosedBehavior.SUSPEND :
                                         settings.LidClosedBehavior.DO_NOTHING);
    settings.recordSettingChange();
  },

  /**
   * @param {!Array<settings.PowerSource>} sources External power sources.
   * @param {string} selectedId The ID of the currently used power source.
   * @param {boolean} lowPowerCharger Whether the currently used power source
   *     is a low-powered USB charger.
   * @private
   */
  powerSourcesChanged_(sources, selectedId, lowPowerCharger) {
    this.powerSources_ = sources;
    this.selectedPowerSourceId_ = selectedId;
    this.lowPowerCharger_ = lowPowerCharger;
  },

  /**
   * @param {settings.LidClosedBehavior} behavior Current behavior.
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
      case settings.LidClosedBehavior.SUSPEND:
      case settings.LidClosedBehavior.DO_NOTHING:
        // "Suspend" and "do nothing" share the "sleep" label and communicate
        // their state via the toggle state.
        this.lidClosedLabel_ = loadTimeData.getString('powerLidSleepLabel');
        pref.value = behavior == settings.LidClosedBehavior.SUSPEND;
        break;
      case settings.LidClosedBehavior.STOP_SESSION:
        this.lidClosedLabel_ = loadTimeData.getString('powerLidSignOutLabel');
        break;
      case settings.LidClosedBehavior.SHUT_DOWN:
        this.lidClosedLabel_ = loadTimeData.getString('powerLidShutDownLabel');
        break;
    }

    if (isControlled) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
    }

    this.lidClosedPref_ = pref;
  },

  /**
   * @param {!settings.IdleBehavior} idleBehavior
   * @param {!settings.IdleBehavior} currIdleBehavior
   * @return {{value: settings.IdleBehavior, name: string, selected:boolean }}
   *     Idle option object that maps to idleBehavior.
   * @private
   */
  getIdleOption_(idleBehavior, currIdleBehavior) {
    const selected = idleBehavior == currIdleBehavior;
    switch (idleBehavior) {
      case settings.IdleBehavior.DISPLAY_OFF_SLEEP:
        return {
          value: idleBehavior,
          name: loadTimeData.getString('powerIdleDisplayOffSleep'),
          selected: selected
        };
      case settings.IdleBehavior.DISPLAY_OFF:
        return {
          value: idleBehavior,
          name: loadTimeData.getString('powerIdleDisplayOff'),
          selected: selected
        };
      case settings.IdleBehavior.DISPLAY_ON:
        return {
          value: idleBehavior,
          name: loadTimeData.getString('powerIdleDisplayOn'),
          selected: selected
        };
      case settings.IdleBehavior.SHUT_DOWN:
        return {
          value: idleBehavior,
          name: loadTimeData.getString('powerIdleDisplayShutDown'),
          selected: selected
        };
      case settings.IdleBehavior.STOP_SESSION:
        return {
          value: idleBehavior,
          name: loadTimeData.getString('powerIdleDisplayStopSession'),
          selected: selected
        };
      default:
        assertNotReached('Unknown IdleBehavior type');
    }
  },

  /**
   * @param {!Array<!settings.IdleBehavior>} acIdleBehaviors
   * @param {!Array<!settings.IdleBehavior>} batteryIdleBehaviors
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
  },

  /**
   * @param {!settings.PowerManagementSettings} powerManagementSettings Current
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
  },

  /**
   * @param {boolean} batteryPresent if battery is present
   * @return {string} 'first' if idle/lid settings are first visible div
   * @private
   */
  getFirst_(batteryPresent) {
    return !batteryPresent ? 'first' : '';
  },

  /**
   * @param {*} lhs
   * @param {*} rhs
   * @return {boolean}
   * @private
   */
  isEqual_(lhs, rhs) {
    return lhs === rhs;
  },
});
