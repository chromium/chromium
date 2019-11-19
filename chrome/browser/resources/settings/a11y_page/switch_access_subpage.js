// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'switch-access-subpage' is the collapsible section containing
 * Switch Access settings.
 */

(function() {

/**
 * Available switch assignment values.
 * @enum {number}
 * @const
 */
const SwitchAccessAssignmentValue = {
  NONE: 0,
  SPACE: 1,
  ENTER: 2,
};

/** @type {!Array<number>} */
const AUTO_SCAN_SPEED_RANGE_MS = [
  500,  600,  700,  800,  900,  1000, 1100, 1200, 1300, 1400, 1500, 1600,
  1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800,
  2900, 3000, 3100, 3200, 3300, 3400, 3500, 3600, 3700, 3800, 3900, 4000
];

/**
 * @param {!Array<number>} ticksInMs
 * @return {!Array<!cr_slider.SliderTick>}
 */
function ticksWithLabelsInSec(ticksInMs) {
  // Dividing by 1000 to convert milliseconds to seconds for the label.
  return ticksInMs.map(x => ({label: `${x / 1000}`, value: x}));
}

Polymer({
  is: 'settings-switch-access-subpage',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
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
      value: function() {
        // navigator.language actually returns a locale, not just a language.
        const locale = window.navigator.language;
        const options = {minimumFractionDigits: 1, maximumFractionDigits: 1};
        return new Intl.NumberFormat(locale, options);
      },
    },

    /** @private {number} */
    maxScanSpeedMs_: {readOnly: true, type: Number, value: 4000},

    /** @private {string} */
    maxScanSpeedLabelSec_: {
      readOnly: true,
      type: String,
      value: function() {
        return this.scanSpeedStringInSec_(this.maxScanSpeedMs_);
      },
    },

    /** @private {number} */
    minScanSpeedMs_: {readOnly: true, type: Number, value: 500},

    /** @private {string} */
    minScanSpeedLabelSec_: {
      readOnly: true,
      type: String,
      value: function() {
        return this.scanSpeedStringInSec_(this.minScanSpeedMs_);
      },
    },

    /** @private {Array<Object>} */
    switchAssignOptions_: {
      readOnly: true,
      type: Array,
      value: function() {
        return [
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
        ];
      },
    },
  },

  /**
   * @return {string}
   * @private
   */
  currentSpeed_: function() {
    const speed = this.get('prefs.switch_access.auto_scan.speed_ms.value');
    if (typeof speed != 'number') {
      return '';
    }
    return this.scanSpeedStringInSec_(speed);
  },

  /**
   * @return {string} label for the speed slider.
   * @private
   */
  getLabelForSpeedSlider_: function() {
    const speedString = this.currentSpeed_();
    return this.i18n('switchAccessAutoScanSpeedLabel', speedString);
  },

  /**
   * @return {boolean} Whether to show settings for auto-scan within the
   *     keyboard.
   * @private
   */
  showKeyboardScanSettings_: function() {
    const improvedTextInputEnabled = loadTimeData.getBoolean(
        'showExperimentalAccessibilitySwitchAccessImprovedTextInput');
    const autoScanEnabled = /** @type {boolean} */
        (this.getPref('switch_access.auto_scan.enabled').value);
    return improvedTextInputEnabled && autoScanEnabled;
  },

  /**
   * @param {string} command
   */
  onSwitchAssigned_: function(command) {
    const pref = 'prefs.switch_access.' + command;
    const keyCodeSuffix = '.key_codes.value';
    const settingSuffix = '.setting.value';

    switch (this.get(pref + settingSuffix)) {
      case SwitchAccessAssignmentValue.NONE:
        this.set(pref + keyCodeSuffix, []);
        break;
      case SwitchAccessAssignmentValue.SPACE:
        this.set(pref + keyCodeSuffix, [32]);
        break;
      case SwitchAccessAssignmentValue.ENTER:
        this.set(pref + keyCodeSuffix, [13]);
        break;
    }
  },

  onNextAssigned_: function() {
    this.onSwitchAssigned_('next');
  },

  onPreviousAssigned_: function() {
    this.onSwitchAssigned_('previous');
  },

  onSelectAssigned_: function() {
    this.onSwitchAssigned_('select');
  },

  /**
   * @param {number} scanSpeedValueMs
   * @return {string} a string representing the scan speed in seconds.
   * @private
   */
  scanSpeedStringInSec_: function(scanSpeedValueMs) {
    const scanSpeedValueSec = scanSpeedValueMs / 1000;
    return this.i18n(
        'durationInSeconds', this.formatter_.format(scanSpeedValueSec));
  },
});
})();
