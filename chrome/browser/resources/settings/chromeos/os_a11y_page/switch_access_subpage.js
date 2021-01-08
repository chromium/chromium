// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'switch-access-subpage' is the collapsible section containing
 * Switch Access settings.
 */

/**
 * The portion of the setting name common to all Switch Access preferences.
 * @const
 */
const PREFIX = 'settings.a11y.switch_access.';

/** @type {!Array<number>} */
const AUTO_SCAN_SPEED_RANGE_MS = [
  700,  800,  900,  1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800,
  1900, 2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900, 3000,
  3100, 3200, 3300, 3400, 3500, 3600, 3700, 3800, 3900, 4000
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
    DeepLinkingBehavior,
    I18nBehavior,
    PrefsBehavior,
    settings.RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private {!Array<{key: string, device: !SwitchAccessDeviceType}>} */
    selectAssignments_: {
      type: Array,
      value: [],
      notify: true,
    },

    /** @private {!Array<{key: string, device: !SwitchAccessDeviceType}>} */
    nextAssignments_: {
      type: Array,
      value: [],
      notify: true,
    },

    /** @private {!Array<{key: string, device: !SwitchAccessDeviceType}>} */
    previousAssignments_: {
      type: Array,
      value: [],
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

    /** @private {boolean} */
    showSwitchAccessActionAssignmentDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {?SwitchAccessCommand} */
    action_: {
      type: String,
      value: null,
      notify: true,
    },
  },

  /** @private {?SwitchAccessSubpageBrowserProxy} */
  switchAccessBrowserProxy_: null,

  /** @override */
  created() {
    this.switchAccessBrowserProxy_ =
        SwitchAccessSubpageBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'switch-access-assignments-changed',
        this.onAssignmentsChanged_.bind(this));
    this.switchAccessBrowserProxy_.refreshAssignmentsFromPrefs();
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

  /** @private */
  onSelectAssignClick_() {
    this.action_ = SwitchAccessCommand.SELECT;
    this.showSwitchAccessActionAssignmentDialog_ = true;
    this.focusAfterDialogClose_ = this.$.selectLinkRow;
  },

  /** @private */
  onNextAssignClick_() {
    this.action_ = SwitchAccessCommand.NEXT;
    this.showSwitchAccessActionAssignmentDialog_ = true;
    this.focusAfterDialogClose_ = this.$.nextLinkRow;
  },

  /** @private */
  onPreviousAssignClick_() {
    this.action_ = SwitchAccessCommand.PREVIOUS;
    this.showSwitchAccessActionAssignmentDialog_ = true;
    this.focusAfterDialogClose_ = this.$.previousLinkRow;
  },

  /** @private */
  onSwitchAccessActionAssignmentDialogClose_() {
    this.showSwitchAccessActionAssignmentDialog_ = false;
    this.focusAfterDialogClose_.focus();
  },

  /**
   * @param {!Object<SwitchAccessCommand, !Array<{key: string, device:
   *     !SwitchAccessDeviceType}>>} value
   * @private
   */
  onAssignmentsChanged_(value) {
    this.selectAssignments_ = value[SwitchAccessCommand.SELECT];
    this.nextAssignments_ = value[SwitchAccessCommand.NEXT];
    this.previousAssignments_ = value[SwitchAccessCommand.PREVIOUS];
  },

  /**
   * @param {{key: string, device: !SwitchAccessDeviceType}} assignment
   * @return {string}
   * @private
   */
  getLabelForAssignment_(assignment) {
    return getLabelForAssignment(assignment);
  },

  /**
   * @param {!Array<{key: string, device: !SwitchAccessDeviceType}>} assignments
   *     List of assignments
   * @return {string} (e.g. 'Alt (USB), Backspace, Enter, and 4 more switches')
   * @private
   */
  getAssignSwitchSubLabel_(assignments) {
    const switches =
        assignments.map(assignment => this.getLabelForAssignment_(assignment));
    switch (switches.length) {
      case 0:
        return this.i18n('assignSwitchSubLabel0Switches');
      case 1:
        return this.i18n('assignSwitchSubLabel1Switch', switches[0]);
      case 2:
        return this.i18n('assignSwitchSubLabel2Switches', ...switches);
      case 3:
        return this.i18n('assignSwitchSubLabel3Switches', ...switches);
      case 4:
        return this.i18n(
            'assignSwitchSubLabel4Switches', ...switches.slice(0, 3));
      default:
        return this.i18n(
            'assignSwitchSubLabel5OrMoreSwitches', ...switches.slice(0, 3),
            switches.length - 3);
    }
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
