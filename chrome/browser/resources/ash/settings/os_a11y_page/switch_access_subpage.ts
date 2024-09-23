// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'switch-access-subpage' is the collapsible section containing
 * Switch Access settings.
 */

import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/settings_slider.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import './switch_access_action_assignment_dialog.js';
import './switch_access_setup_guide_dialog.js';
import './switch_access_setup_guide_warning_dialog.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrLinkRowElement} from 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import {SliderTick} from 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, routes} from '../router.js';

import {AUTO_SCAN_SPEED_RANGE_MS, SwitchAccessCommand, SwitchAccessDeviceType} from './switch_access_constants.js';
import {getTemplate} from './switch_access_subpage.html.js';
import {SwitchAccessSubpageBrowserProxy, SwitchAccessSubpageBrowserProxyImpl} from './switch_access_subpage_browser_proxy.js';
import {KeyAssignment, SwitchAccessAssignmentsChangedValue} from './switch_access_types.js';

/**
 * The portion of the setting name common to all Switch Access preferences.
 */
const PREFIX = 'settings.a11y.switch_access.';

const POINT_SCAN_SPEED_RANGE_DIPS_PER_SECOND: number[] =
    [25, 50, 75, 100, 150, 200, 300];

function ticksWithLabelsInSec(ticksInMs: number[]): SliderTick[] {
  // Dividing by 1000 to convert milliseconds to seconds for the label.
  return ticksInMs.map(x => ({label: `${x / 1000}`, value: x}));
}

function ticksWithCountingLabels(ticks: number[]): SliderTick[] {
  return ticks.map((x, i) => ({label: `${i + 1}`, value: x}));
}

export interface SettingsSwitchAccessSubpageElement {
  $: {
    nextLinkRow: CrLinkRowElement,
    previousLinkRow: CrLinkRowElement,
    selectLinkRow: CrLinkRowElement,
    setupGuideLink: CrLinkRowElement,
  };
}

const SettingsSwitchAccessSubpageElementBase = DeepLinkingMixin(PrefsMixin(
    RouteObserverMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class SettingsSwitchAccessSubpageElement extends
    SettingsSwitchAccessSubpageElementBase {
  static get is() {
    return 'settings-switch-access-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectAssignments_: {
        type: Array,
        value: [],
        notify: true,
      },

      nextAssignments_: {
        type: Array,
        value: [],
        notify: true,
      },

      previousAssignments_: {
        type: Array,
        value: [],
        notify: true,
      },

      autoScanSpeedRangeMs_: {
        readOnly: true,
        type: Array,
        value: ticksWithLabelsInSec(AUTO_SCAN_SPEED_RANGE_MS),
      },

      pointScanSpeedRangeDipsPerSecond_: {
        readOnly: true,
        type: Array,
        value: ticksWithCountingLabels(POINT_SCAN_SPEED_RANGE_DIPS_PER_SECOND),
      },

      formatter_: {
        type: Object,
        value() {
          // navigator.language actually returns a locale, not just a language.
          const locale = window.navigator.language;
          const options = {minimumFractionDigits: 1, maximumFractionDigits: 1};
          return new Intl.NumberFormat(locale, options);
        },
      },

      maxScanSpeedMs_: {
        readOnly: true,
        type: Number,
        value: AUTO_SCAN_SPEED_RANGE_MS[AUTO_SCAN_SPEED_RANGE_MS.length - 1],
      },

      maxScanSpeedLabelSec_: {
        readOnly: true,
        type: String,
      },

      minScanSpeedMs_: {
        readOnly: true,
        type: Number,
        value: AUTO_SCAN_SPEED_RANGE_MS[0],
      },

      minScanSpeedLabelSec_: {
        readOnly: true,
        type: String,
      },

      maxPointScanSpeed_: {
        readOnly: true,
        type: Number,
        value: POINT_SCAN_SPEED_RANGE_DIPS_PER_SECOND.length,
      },

      minPointScanSpeed_: {
        readOnly: true,
        type: Number,
        value: 1,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kSwitchActionAssignment,
          Setting.kSwitchActionAutoScan,
          Setting.kSwitchActionAutoScanKeyboard,
        ]),
      },

      showSwitchAccessActionAssignmentDialog_: {
        type: Boolean,
        value: false,
      },

      showSwitchAccessSetupGuideDialog_: {
        type: Boolean,
        value: false,
      },

      showSwitchAccessSetupGuideWarningDialog_: {
        type: Boolean,
        value: false,
      },

      action_: {
        type: String,
        value: null,
        notify: true,
      },
    };
  }

  private action_: SwitchAccessCommand|null;
  private autoScanSpeedRangeMs_: number[];
  private focusAfterDialogClose_: HTMLElement|null;
  private formatter_: Intl.NumberFormat;
  private maxPointScanSpeed_: number;
  private minPointScanSpeed_: number;
  private maxScanSpeedLabelSec_: string;
  private maxScanSpeedMs_: number;
  private minScanSpeedLabelSec_: string;
  private minScanSpeedMs_: number;
  private nextAssignments_: KeyAssignment[];
  private pointScanSpeedRangeDipsPerSecond_: number[];
  private previousAssignments_: KeyAssignment[];
  private selectAssignments_: KeyAssignment[];
  private showSwitchAccessActionAssignmentDialog_: boolean;
  private showSwitchAccessSetupGuideDialog_: boolean;
  private showSwitchAccessSetupGuideWarningDialog_: boolean;
  private switchAccessBrowserProxy_: SwitchAccessSubpageBrowserProxy;

  constructor() {
    super();

    this.maxScanSpeedLabelSec_ =
        this.scanSpeedStringInSec_(this.maxScanSpeedMs_);
    this.minScanSpeedLabelSec_ =
        this.scanSpeedStringInSec_(this.minScanSpeedMs_);
    this.switchAccessBrowserProxy_ =
        SwitchAccessSubpageBrowserProxyImpl.getInstance();

    this.focusAfterDialogClose_ = null;
  }

  override ready(): void {
    super.ready();

    this.addWebUiListener(
        'switch-access-assignments-changed',
        (value: SwitchAccessAssignmentsChangedValue) =>
            this.onAssignmentsChanged_(value));
    this.switchAccessBrowserProxy_.refreshAssignmentsFromPrefs();
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.MANAGE_SWITCH_ACCESS_SETTINGS) {
      return;
    }

    this.attemptDeepLink();
  }

  private onSetupGuideRerunClick_(): void {
    this.showSwitchAccessSetupGuideWarningDialog_ = true;
  }

  private onSetupGuideWarningDialogCancel_(): void {
    this.showSwitchAccessSetupGuideWarningDialog_ = false;
  }

  private onSetupGuideWarningDialogClose_(): void {
    // The on_cancel is followed by on_close, so check cancel didn't happen
    // first.
    if (this.showSwitchAccessSetupGuideWarningDialog_) {
      this.openSetupGuide_();
      this.showSwitchAccessSetupGuideWarningDialog_ = false;
    }
  }

  private openSetupGuide_(): void {
    this.showSwitchAccessSetupGuideWarningDialog_ = false;
    this.showSwitchAccessSetupGuideDialog_ = true;
  }

  private onSelectAssignClick_(): void {
    this.action_ = SwitchAccessCommand.SELECT;
    this.showSwitchAccessActionAssignmentDialog_ = true;
    this.focusAfterDialogClose_ = this.$.selectLinkRow;
  }

  private onNextAssignClick_(): void {
    this.action_ = SwitchAccessCommand.NEXT;
    this.showSwitchAccessActionAssignmentDialog_ = true;
    this.focusAfterDialogClose_ = this.$.nextLinkRow;
  }

  private onPreviousAssignClick_(): void {
    this.action_ = SwitchAccessCommand.PREVIOUS;
    this.showSwitchAccessActionAssignmentDialog_ = true;
    this.focusAfterDialogClose_ = this.$.previousLinkRow;
  }

  private onSwitchAccessSetupGuideDialogClose_(): void {
    this.showSwitchAccessSetupGuideDialog_ = false;
    this.$.setupGuideLink.focus();
  }

  private onSwitchAccessActionAssignmentDialogClose_(): void {
    this.showSwitchAccessActionAssignmentDialog_ = false;
    this.focusAfterDialogClose_!.focus();
  }

  private onAssignmentsChanged_(value: SwitchAccessAssignmentsChangedValue):
      void {
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

  private getLabelForDeviceType_(deviceType: SwitchAccessDeviceType):
      TrustedHTML {
    switch (deviceType) {
      case SwitchAccessDeviceType.INTERNAL:
        return this.i18nAdvanced('switchAccessInternalDeviceTypeLabel', {});
      case SwitchAccessDeviceType.USB:
        return this.i18nAdvanced('switchAccessUsbDeviceTypeLabel', {});
      case SwitchAccessDeviceType.BLUETOOTH:
        return this.i18nAdvanced('switchAccessBluetoothDeviceTypeLabel', {});
      case SwitchAccessDeviceType.UNKNOWN:
        return this.i18nAdvanced('switchAccessUnknownDeviceTypeLabel', {});
      default:
        assertNotReached('Invalid device type.');
    }
  }

  /**
   * Converts assignment object to pretty-formatted label.
   * E.g. {key: 'Escape', device: 'usb'} -> 'Escape (USB)'
   */
  private getLabelForAssignment_(assignment: KeyAssignment): TrustedHTML {
    return this.i18nAdvanced('switchAndDeviceType', {
      substitutions: [
        assignment.key,
        this.getLabelForDeviceType_(assignment.device).toString(),
      ],
    });
  }

  /**
   * @return (e.g. 'Alt (USB), Backspace, Enter, and 4 more switches')
   */
  private getAssignSwitchSubLabel_(assignments: KeyAssignment[]): string {
    const switches = assignments.map(
        assignment => this.getLabelForAssignment_(assignment).toString());
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

  private showKeyboardScanSettings_(): boolean {
    const improvedTextInputEnabled = loadTimeData.getBoolean(
        'showExperimentalAccessibilitySwitchAccessImprovedTextInput');

    const pref = this.getPref<boolean>(PREFIX + 'auto_scan.enabled');
    const autoScanEnabled = pref.value;
    return improvedTextInputEnabled && autoScanEnabled;
  }

  private scanSpeedStringInSec_(scanSpeedValueMs: number): string {
    const scanSpeedValueSec = scanSpeedValueMs / 1000;
    return this.i18n(
        'durationInSeconds', this.formatter_.format(scanSpeedValueSec));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-switch-access-subpage': SettingsSwitchAccessSubpageElement;
  }
}

customElements.define(
    SettingsSwitchAccessSubpageElement.is, SettingsSwitchAccessSubpageElement);
