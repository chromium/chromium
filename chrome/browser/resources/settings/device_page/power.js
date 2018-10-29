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
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    enablePowerSettings: Boolean,

    /** @private {string} ID of the selected power source, or ''. */
    selectedPowerSourceId_: String,

    /** @private {!settings.BatteryStatus|undefined} */
    batteryStatus_: Object,

    /** @private {boolean} Whether a low-power (USB) charger is being used. */
    lowPowerCharger_: Boolean,

    /** @private {boolean} Whether the idle behavior is controlled by policy. */
    idleControlled_: Boolean,

    /** @private {string} Text for label describing the lid-closed behavior. */
    lidClosedLabel_: String,

    /** @private {boolean} Whether the system possesses a lid. */
    hasLid_: Boolean,

    /**
     * List of available dual-role power sources, if enablePowerSettings is on.
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

    /** @private */
    idleOptions_: {
      type: Array,
      computed: 'computeIdleOptions_(idleControlled_)',
    },

    /** @private {!chrome.settingsPrivate.PrefObject} */
    lidClosedPref_: {
      type: Object,
      value: function() {
        return /** @type {!chrome.settingsPrivate.PrefObject} */ ({});
      },
    },
  },

  /** @override */
  ready: function() {
    // enablePowerSettings comes from loadTimeData, so it will always be set
    // before attached() is called.
    if (!this.enablePowerSettings)
      settings.navigateToPreviousRoute();
  },

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'battery-status-changed', this.set.bind(this, 'batteryStatus_'));
    this.addWebUIListener(
        'power-sources-changed', this.powerSourcesChanged_.bind(this));
    settings.DevicePageBrowserProxyImpl.getInstance().updatePowerStatus();

    this.addWebUIListener(
        'power-management-settings-changed',
        this.powerManagementSettingsChanged_.bind(this));
    settings.DevicePageBrowserProxyImpl.getInstance()
        .requestPowerManagementSettings();
  },

  /**
   * @param {!Array<!settings.PowerSource>|undefined} powerSources
   * @param {boolean} calculating
   * @return {string} The primary label for the power source row.
   * @private
   */
  computePowerSourceLabel_: function(powerSources, calculating) {
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
  computeShowPowerSourceDropdown_: function(powerSources) {
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
  computePowerSourceName_: function(powerSources, lowPowerCharger) {
    if (lowPowerCharger)
      return this.i18n('powerSourceLowPowerCharger');
    if (powerSources.length)
      return this.i18n('powerSourceAcAdapter');
    return '';
  },

  /**
   * @param {boolean} idleControlled
   * @return {!Array<!{value: settings.IdleBehavior, name: string}>} Options to
   *     display in idle-behavior select.
   * @private
   */
  computeIdleOptions_: function(idleControlled) {
    const options = [
      {
        value: settings.IdleBehavior.DISPLAY_OFF_SLEEP,
        name: loadTimeData.getString('powerIdleDisplayOffSleep'),
      },
      {
        value: settings.IdleBehavior.DISPLAY_OFF,
        name: loadTimeData.getString('powerIdleDisplayOff'),
      },
      {
        value: settings.IdleBehavior.DISPLAY_ON,
        name: loadTimeData.getString('powerIdleDisplayOn'),
      },
    ];
    if (idleControlled) {
      options.push({
        value: settings.IdleBehavior.OTHER,
        name: loadTimeData.getString('powerIdleOther'),
      });
    }
    return options;
  },

  /** @private */
  onPowerSourceChange_: function() {
    settings.DevicePageBrowserProxyImpl.getInstance().setPowerSource(
        this.$.powerSource.value);
  },

  /** @private */
  onIdleSelectChange_: function() {
    const behavior = /** @type {settings.IdleBehavior} */
        (parseInt(this.$.idleSelect.value, 10));
    settings.DevicePageBrowserProxyImpl.getInstance().setIdleBehavior(behavior);
  },

  /** @private */
  onLidClosedToggleChange_: function() {
    // Other behaviors are only displayed when the setting is controlled, in
    // which case the toggle can't be changed by the user.
    settings.DevicePageBrowserProxyImpl.getInstance().setLidClosedBehavior(
        this.$.lidClosedToggle.checked ? settings.LidClosedBehavior.SUSPEND :
                                         settings.LidClosedBehavior.DO_NOTHING);
  },

  /**
   * @param {!Array<settings.PowerSource>} sources External power sources.
   * @param {string} selectedId The ID of the currently used power source.
   * @param {boolean} lowPowerCharger Whether the currently used power source
   *     is a low-powered USB charger.
   * @private
   */
  powerSourcesChanged_: function(sources, selectedId, lowPowerCharger) {
    this.powerSources_ = sources;
    this.selectedPowerSourceId_ = selectedId;
    this.lowPowerCharger_ = lowPowerCharger;
  },

  /**
   * @param {settings.LidClosedBehavior} behavior Current behavior.
   * @param {boolean} isControlled Whether the underlying pref is controlled.
   * @private
   */
  updateLidClosedLabelAndPref_: function(behavior, isControlled) {
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
   * @param {!settings.PowerManagementSettings} browserSettings Current power
   *     management settings.
   * @private
   */
  powerManagementSettingsChanged_: function(browserSettings) {
    this.idleControlled_ = browserSettings.idleControlled;
    this.hasLid_ = browserSettings.hasLid;
    this.updateLidClosedLabelAndPref_(
        browserSettings.lidClosedBehavior, browserSettings.lidClosedControlled);

    // The idle behavior select element includes an "Other" option when
    // controlled but omits it otherwise. Make sure that the option is there
    // before we potentially try to select it.
    this.async(function() {
      this.$.idleSelect.value = browserSettings.idleBehavior;
    });
  },

  /**
   * @param {boolean} batteryPresent if battery is present
   * @return {string} 'first' if idle/lid settings are first visible div
   * @private
   */
  getFirst_: function(batteryPresent) {
    return !batteryPresent ? 'first' : '';
  },

  /**
   * @param {*} lhs
   * @param {*} rhs
   * @return {boolean}
   * @private
   */
  isEqual_: function(lhs, rhs) {
    return lhs === rhs;
  },
});
