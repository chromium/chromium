// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'switch-access-subpage' is the collapsible section containing
 * Switch Access settings.
 */

import 'chrome://resources/cr_elements/md_select_css.m.js';
import '../../controls/settings_slider.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared_css.js';
import './switch_access_action_assignment_dialog.js';
import './switch_access_setup_guide_dialog.js';
import './switch_access_setup_guide_warning_dialog.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {getLabelForAssignment} from './switch_access_action_assignment_pane.js';
import {AUTO_SCAN_SPEED_RANGE_MS, SwitchAccessCommand, SwitchAccessDeviceType} from './switch_access_constants.js';
import {SwitchAccessSubpageBrowserProxy, SwitchAccessSubpageBrowserProxyImpl} from './switch_access_subpage_browser_proxy.js';

/**
 * The portion of the setting name common to all Switch Access preferences.
 * @const
 */
const PREFIX = 'settings.a11y.switch_access.';

/** @type {!Array<number>} */
const POINT_SCAN_SPEED_RANGE_DIPS_PER_SECOND = [25, 50, 75, 100, 150, 200, 300];

/**
 * @param {!Array<number>} ticksInMs
 * @return {!Array<!SliderTick>}
 */
function ticksWithLabelsInSec(ticksInMs) {
  // Dividing by 1000 to convert milliseconds to seconds for the label.
  return ticksInMs.map(x => ({label: `${x / 1000}`, value: x}));
}

/**
 * @param {!Array<number>} ticks
 * @return {!Array<!SliderTick>}
 */
function ticksWithCountingLabels(ticks) {
  return ticks.map((x, i) => ({label: i + 1, value: x}));
}

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsSwitchAccessSubpageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior, I18nBehavior, PrefsBehavior, RouteObserverBehavior,
      WebUIListenerBehavior
    ],
    PolymerElement);

/** @polymer */
class SettingsSwitchAccessSubpageElement extends
    SettingsSwitchAccessSubpageElementBase {
  static get is() {
    return 'settings-switch-access-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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

      /** @private {Array<number>} */
      pointScanSpeedRangeDipsPerSecond_: {
        readOnly: true,
        type: Array,
        value: ticksWithCountingLabels(POINT_SCAN_SPEED_RANGE_DIPS_PER_SECOND),
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

      /** @private {number} */
      maxPointScanSpeed_: {
        readOnly: true,
        type: Number,
        value: POINT_SCAN_SPEED_RANGE_DIPS_PER_SECOND.length
      },

      /** @private {number} */
      minPointScanSpeed_: {readOnly: true, type: Number, value: 1},

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

      /** @private */
      showSwitchAccessActionAssignmentDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      showSwitchAccessSetupGuideDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      showSwitchAccessSetupGuideWarningDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private {?SwitchAccessCommand} */
      action_: {
        type: String,
        value: null,
        notify: true,
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!SwitchAccessSubpageBrowserProxy} */
    this.switchAccessBrowserProxy_ =
        SwitchAccessSubpageBrowserProxyImpl.getInstance();

    /** @private {?HTMLElement} */
    this.focusAfterDialogClose_ = null;
  }

  /** @override */
  ready() {
    super.ready();

    this.addWebUIListener(
        'switch-access-assignments-changed',
        value => this.onAssignmentsChanged_(value));
    this.switchAccessBrowserProxy_.refreshAssignmentsFromPrefs();
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.MANAGE_SWITCH_ACCESS_SETTINGS) {
      return;
    }

    this.attemptDeepLink();
  }

  /** @private */
  onSetupGuideRerunClick_() {
    this.showSwitchAccessSetupGuideWarningDialog_ = true;
  }

  /** @private */
  onSetupGuideWarningDialogCancel_() {
    this.showSwitchAccessSetupGuideWarningDialog_ = false;
  }

  /** @private */
  onSetupGuideWarningDialogClose_() {
    // The on_cancel is followed by on_close, so check cancel didn't happen
    // first.
    if (this.showSwitchAccessSetupGuideWarningDialog_) {
      this.openSetupGuide_();
      this.showSwitchAccessSetupGuideWarningDialog_ = false;
    }
  }

  /** @private */
  openSetupGuide_() {
    this.showSwitchAccessSetupGuideWarningDialog_ = false;
    this.showSwitchAccessSetupGuideDialog_ = true;
  }

  /** @private */
  onSelectAssignClick_() {
    this.action_ = SwitchAccessCommand.SELECT;
    this.showSwitchAccessActionAssignmentDialog_ = true;
    this.focusAfterDialogClose_ =
        /** @type {?HTMLElement} */ (this.$.selectLinkRow);
  }

  /** @private */
  onNextAssignClick_() {
    this.action_ = SwitchAccessCommand.NEXT;
    this.showSwitchAccessActionAssignmentDialog_ = true;
    this.focusAfterDialogClose_ =
        /** @type {?HTMLElement} */ (this.$.nextLinkRow);
  }

  /** @private */
  onPreviousAssignClick_() {
    this.action_ = SwitchAccessCommand.PREVIOUS;
    this.showSwitchAccessActionAssignmentDialog_ = true;
    this.focusAfterDialogClose_ =
        /** @type {?HTMLElement} */ (this.$.previousLinkRow);
  }

  /** @private */
  onSwitchAccessSetupGuideDialogClose_() {
    this.showSwitchAccessSetupGuideDialog_ = false;
    this.$.setupGuideLink.focus();
  }

  /** @private */
  onSwitchAccessActionAssignmentDialogClose_() {
    this.showSwitchAccessActionAssignmentDialog_ = false;
    this.focusAfterDialogClose_.focus();
  }

  /**
   * @param {!Object<SwitchAccessCommand, !Array<{key: string, device:
   *     !SwitchAccessDeviceType}>>} value
   * @private
   */
  onAssignmentsChanged_(value) {
    this.selectAssignments_ = value[SwitchAccessCommand.SELECT];
    this.nextAssignments_ = value[SwitchAccessCommand.NEXT];
    this.previousAssignments_ = value[SwitchAccessCommand.PREVIOUS];

    // Any complete assignment will have at least one switch assigned to SELECT.
    // If this method is called with no SELECT switches, then the page has just
    // loaded, and we should open the setup guide.
    if (Object.keys(this.selectAssignments_).length === 0) {
      this.openSetupGuide_();
    }
  }

  /**
   * @param {{key: string, device: !SwitchAccessDeviceType}} assignment
   * @return {string}
   * @private
   */
  getLabelForAssignment_(assignment) {
    return getLabelForAssignment(assignment);
  }

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
  }

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
  }

  /**
   * @param {number} scanSpeedValueMs
   * @return {string} a string representing the scan speed in seconds.
   * @private
   */
  scanSpeedStringInSec_(scanSpeedValueMs) {
    const scanSpeedValueSec = scanSpeedValueMs / 1000;
    return this.i18n(
        'durationInSeconds', this.formatter_.format(scanSpeedValueSec));
  }
}

customElements.define(
    SettingsSwitchAccessSubpageElement.is, SettingsSwitchAccessSubpageElement);
