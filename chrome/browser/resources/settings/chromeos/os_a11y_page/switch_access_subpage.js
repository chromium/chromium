// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'switch-access-subpage' is the collapsible section containing
 * Switch Access settings.
 */

(function() {

/**
 * The portion of the setting name common to all Switch Access preferences.
 * @const
 */
const PREFIX = 'settings.a11y.switch_access.';

/**
 * The ending of the setting name for all key code preferences.
 * @const
 */
const KEY_CODE_SUFFIX = '.key_codes';

/**
 * The ending of the setting name for all preferences referring to
 * Switch Access command settings.
 * @const
 */
const COMMAND_SUFFIX = '.setting';

/** @type {!Array<number>} */
const AUTO_SCAN_SPEED_RANGE_MS = [
  700,  800,  900,  1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800,
  1900, 2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900, 3000,
  3100, 3200, 3300, 3400, 3500, 3600, 3700, 3800, 3900, 4000
];

/**
 * This function extracts the segment of a preference key after the fixed prefix
 * and returns it. In cases where the preference is Switch Access command
 * setting preference, it corresponds to the command name.
 *
 * @param {!chrome.settingsPrivate.PrefObject} pref
 * @return {string}
 */
function getCommandNameFromCommandPref(pref) {
  const nameStartIndex = PREFIX.length;
  const nameEndIndex = pref.key.indexOf('.', nameStartIndex);
  return pref.key.substring(nameStartIndex, nameEndIndex);
}

/**
 * @param {!Array<number>} ticksInMs
 * @return {!Array<!cr_slider.SliderTick>}
 */
function ticksWithLabelsInSec(ticksInMs) {
  // Dividing by 1000 to convert milliseconds to seconds for the label.
  return ticksInMs.map(x => ({label: `${x / 1000}`, value: x}));
}

/**
 * @param {!Array} array
 * @param {*} value
 * @return {!Array}
 */
function removeElementWithValue(array, value) {
  for (let i = 0; i < array.length; i++) {
    if (array[i].value === value) {
      array.splice(i, 1);
      return array;
    }
  }
  return array;
}

Polymer({
  is: 'settings-switch-access-subpage',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    PrefsBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private {Array<number>} */
    autoScanSpeedRangeMs_: {
      readOnly: true,
      type: Array,
      value: ticksWithLabelsInSec(AUTO_SCAN_SPEED_RANGE_MS),
    },

    /** @private {Object} */
    formatter_: {
      type: Object,
      value() {
        // navigator.language actually returns a locale, not just a language.
        const locale = window.navigator.language;
        const options = {minimumFractionDigits: 1, maximumFractionDigits: 1};
        return new Intl.NumberFormat(locale, options);
      },
    },

    /** @private {number} */
    maxScanSpeedMs_: {
      readOnly: true,
      type: Number,
      value: AUTO_SCAN_SPEED_RANGE_MS[AUTO_SCAN_SPEED_RANGE_MS.length - 1]
    },

    /** @private {string} */
    maxScanSpeedLabelSec_: {
      readOnly: true,
      type: String,
      value() {
        return this.scanSpeedStringInSec_(this.maxScanSpeedMs_);
      },
    },

    /** @private {number} */
    minScanSpeedMs_:
        {readOnly: true, type: Number, value: AUTO_SCAN_SPEED_RANGE_MS[0]},

    /** @private {string} */
    minScanSpeedLabelSec_: {
      readOnly: true,
      type: String,
      value() {
        return this.scanSpeedStringInSec_(this.minScanSpeedMs_);
      },
    },

    /**
     * @private {!Array<{value: !SwitchAccessAssignmentValue, name: string}>}
     */
    optionsForNext_: {
      type: Array,
      value() {
        return [{value: -1, name: this.i18n('switchAssignOptionPlaceholder')}];
      }
    },

    /**
     * @private {!Array<{value: !SwitchAccessAssignmentValue, name: string}>}
     */
    optionsForPrevious_: {
      type: Array,
      value() {
        return [{value: -1, name: this.i18n('switchAssignOptionPlaceholder')}];
      }
    },

    /**
     * @private {!Array<{value: !SwitchAccessAssignmentValue, name: string}>}
     */
    optionsForSelect_: {
      type: Array,
      value() {
        return [{value: -1, name: 'Placeholder'}];
      }
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kSwitchActionAssignment,
        chromeos.settings.mojom.Setting.kSwitchActionAutoScan,
        chromeos.settings.mojom.Setting.kSwitchActionAutoScanKeyboard,
      ]),
    },
  },

  /** @override */
  created() {
    this.initSwitchAssignmentOptions_();
    chrome.settingsPrivate.onPrefsChanged.addListener((prefs) => {
      for (const pref of prefs) {
        if (!pref.key.includes(PREFIX) || !pref.key.includes(COMMAND_SUFFIX)) {
          continue;
        }
        const commandName = getCommandNameFromCommandPref(pref);
        if (Object.values(SwitchAccessCommand).includes(commandName)) {
          this.onSwitchAssigned_(pref);
        }
      }
    });
  },

  /** @override */
  ready() {
    this.updateOptionsForDropdowns_();
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.MANAGE_SWITCH_ACCESS_SETTINGS) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @private {?Array<{value: !SwitchAccessAssignmentValue, name: string}>}
   */
  allSwitchKeyOptions_: null,

  /** @private */
  initSwitchAssignmentOptions_() {
    this.allSwitchKeyOptions_ = [
      {
        value: SwitchAccessAssignmentValue.NONE,
        name: this.i18n('switchAssignOptionNone')
      },
      {
        value: SwitchAccessAssignmentValue.SPACE,
        name: this.i18n('switchAssignOptionSpace')
      },
      {
        value: SwitchAccessAssignmentValue.ENTER,
        name: this.i18n('switchAssignOptionEnter')
      },
      // Arabic numerals are used consistently across languages, so the
      // strings need not be internationalized.
      {value: SwitchAccessAssignmentValue.ONE, name: '1'},
      {value: SwitchAccessAssignmentValue.TWO, name: '2'},
      {value: SwitchAccessAssignmentValue.THREE, name: '3'},
      {value: SwitchAccessAssignmentValue.FOUR, name: '4'},
      {value: SwitchAccessAssignmentValue.FIVE, name: '5'},
    ];
  },

  /**
   * @return {string}
   * @private
   */
  currentSpeed_() {
    const speed = this.getPref(PREFIX + 'auto_scan.speed_ms').value;
    if (typeof speed != 'number') {
      return '';
    }
    return this.scanSpeedStringInSec_(speed);
  },

  /**
   * @return {boolean} Whether to show settings for auto-scan within the
   *     keyboard.
   * @private
   */
  showKeyboardScanSettings_() {
    const improvedTextInputEnabled = loadTimeData.getBoolean(
        'showExperimentalAccessibilitySwitchAccessImprovedTextInput');
    const autoScanEnabled = /** @type {boolean} */
        (this.getPref(PREFIX + 'auto_scan.enabled').value);
    return improvedTextInputEnabled && autoScanEnabled;
  },

  /**
   * @param {!chrome.settingsPrivate.PrefObject} newPref
   * @private
   */
  onSwitchAssigned_(newPref) {
    const command = getCommandNameFromCommandPref(newPref);

    this.updateOptionsForDropdowns_();

    // Because of complexities with mapping a ListPref to a settings-dropdown,
    // we instead store two distinct preferences (one for the dropdown selection
    // and one with the key codes that Switch Access intercepts). The following
    // code sets the key code preference based on the dropdown preference.
    const keyPref = PREFIX + command + KEY_CODE_SUFFIX;
    switch (newPref.value) {
      case SwitchAccessAssignmentValue.NONE:
        chrome.settingsPrivate.setPref(keyPref, []);
        break;
      case SwitchAccessAssignmentValue.SPACE:
        chrome.settingsPrivate.setPref(keyPref, [32]);
        break;
      case SwitchAccessAssignmentValue.ENTER:
        chrome.settingsPrivate.setPref(keyPref, [13]);
        break;
      case SwitchAccessAssignmentValue.ONE:
        chrome.settingsPrivate.setPref(keyPref, [49]);
        break;
      case SwitchAccessAssignmentValue.TWO:
        chrome.settingsPrivate.setPref(keyPref, [50]);
        break;
      case SwitchAccessAssignmentValue.THREE:
        chrome.settingsPrivate.setPref(keyPref, [51]);
        break;
      case SwitchAccessAssignmentValue.FOUR:
        chrome.settingsPrivate.setPref(keyPref, [52]);
        break;
      case SwitchAccessAssignmentValue.FIVE:
        chrome.settingsPrivate.setPref(keyPref, [53]);
        break;
    }
  },

  /**
   * Updates the options available to each command by filtering out options
   * currently used by a different command.
   *
   * @private
   */
  updateOptionsForDropdowns_() {
    if (!this.allSwitchKeyOptions_) {
      return;
    }
    /**
     * Make a copy of the list of all possible options for each command.
     * @type {!Object<!SwitchAccessCommand, !Array<{value:
     *     !SwitchAccessAssignmentValue, name: string}>>}
     */
    const optionsFor = {};
    for (const command of Object.values(SwitchAccessCommand)) {
      optionsFor[command] = [...this.allSwitchKeyOptions_];
    }

    // Remove each key code assigned to a value from the other commands' lists.
    for (const command of Object.values(SwitchAccessCommand)) {
      const value = this.getPref(PREFIX + command + COMMAND_SUFFIX).value;
      if (value === SwitchAccessAssignmentValue.NONE) {
        continue;
      }

      for (const other in optionsFor) {
        if (other === command) {
          continue;
        }
        optionsFor[other] = removeElementWithValue(optionsFor[other], value);
      }
    }

    // Assign the calculated options to the corresponding Polymer property.
    this.optionsForNext_ = optionsFor[SwitchAccessCommand.NEXT];
    this.optionsForPrevious_ = optionsFor[SwitchAccessCommand.PREVIOUS];
    this.optionsForSelect_ = optionsFor[SwitchAccessCommand.SELECT];
  },

  /**
   * @param {number} scanSpeedValueMs
   * @return {string} a string representing the scan speed in seconds.
   * @private
   */
  scanSpeedStringInSec_(scanSpeedValueMs) {
    const scanSpeedValueSec = scanSpeedValueMs / 1000;
    return this.i18n(
        'durationInSeconds', this.formatter_.format(scanSpeedValueSec));
  },
});
})();
