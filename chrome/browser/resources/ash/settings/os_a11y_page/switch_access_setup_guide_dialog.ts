// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog walks a user through the flow of setting up Switch
 * Access.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import '../controls/settings_slider.js';
import '../os_settings_icons.html.js';
import './switch_access_action_assignment_pane.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrRadioGroupElement} from 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import {SliderTick} from 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {Router, routes} from '../router.js';

import {SettingsSwitchAccessActionAssignmentPaneElement} from './switch_access_action_assignment_pane.js';
import {actionToPref, AssignmentContext, AUTO_SCAN_ENABLED_PREF, AUTO_SCAN_KEYBOARD_SPEED_PREF, AUTO_SCAN_SPEED_PREF, AUTO_SCAN_SPEED_RANGE_MS, DEFAULT_AUTO_SCAN_SPEED_MS, SwitchAccessCommand} from './switch_access_constants.js';
import {getTemplate} from './switch_access_setup_guide_dialog.html.js';
import {SwitchAccessSubpageBrowserProxyImpl} from './switch_access_subpage_browser_proxy.js';

/**
 * Elements that can be hidden or shown for each setup page.
 * The string value should match the element ID in the HTML.
 */
enum SetupElement {
  BLUETOOTH_BUTTON = 'bluetooth',
  DONE_BUTTON = 'done',
  NEXT_BUTTON = 'next',
  PREVIOUS_BUTTON = 'previous',
  START_OVER_BUTTON = 'startOver',
  INTRO_CONTENT = 'intro',
  ASSIGN_SWITCH_CONTENT = 'assignSwitch',
  AUTO_SCAN_ENABLED_CONTENT = 'autoScanEnabled',
  CHOOSE_SWITCH_COUNT_CONTENT = 'chooseSwitchCount',
  AUTO_SCAN_SPEED_CONTENT = 'autoScanSpeed',
  CLOSING_CONTENT = 'closing',
}

/**
 * The IDs of each page in the setup flow.
 */
enum SetupPageId {
  INTRO = 0,
  ASSIGN_SELECT = 1,
  AUTO_SCAN_ENABLED = 2,
  CHOOSE_SWITCH_COUNT = 3,
  AUTO_SCAN_SPEED = 4,
  ASSIGN_NEXT = 5,
  ASSIGN_PREVIOUS = 6,
  TIC_TAC_TOE = 7,
  CLOSING = 8,
}

/**
 * Defines what is visible onscreen for a given page of the setup guide.
 */
interface SetupPage {
  titleId: string;
  visibleElements: SetupElement[];
}

/**
 * A dictionary of all of the dialog pages.
 */
const SetupPageList: {[key in SetupPageId]?: SetupPage} = {
  [SetupPageId.INTRO]: {
    titleId: 'switchAccessSetupIntroTitle',
    visibleElements: [
      SetupElement.BLUETOOTH_BUTTON,
      SetupElement.NEXT_BUTTON,
      SetupElement.INTRO_CONTENT,
    ],
  },

  [SetupPageId.ASSIGN_SELECT]: {
    titleId: 'switchAccessSetupAssignSelectTitle',
    visibleElements: [SetupElement.ASSIGN_SWITCH_CONTENT],
  },

  [SetupPageId.AUTO_SCAN_ENABLED]: {
    titleId: 'switchAccessSetupAutoScanEnabledTitle',
    visibleElements: [
      SetupElement.NEXT_BUTTON,
      SetupElement.PREVIOUS_BUTTON,
      SetupElement.AUTO_SCAN_ENABLED_CONTENT,
    ],
  },

  [SetupPageId.CHOOSE_SWITCH_COUNT]: {
    titleId: 'switchAccessSetupChooseSwitchCountTitle',
    visibleElements: [
      SetupElement.NEXT_BUTTON,
      SetupElement.PREVIOUS_BUTTON,
      SetupElement.CHOOSE_SWITCH_COUNT_CONTENT,
    ],
  },

  [SetupPageId.AUTO_SCAN_SPEED]: {
    titleId: 'switchAccessSetupAutoScanSpeedTitle',
    visibleElements: [
      SetupElement.NEXT_BUTTON,
      SetupElement.PREVIOUS_BUTTON,
      SetupElement.AUTO_SCAN_SPEED_CONTENT,
    ],
  },

  [SetupPageId.ASSIGN_NEXT]: {
    titleId: 'switchAccessSetupAssignNextTitle',
    visibleElements:
        [SetupElement.PREVIOUS_BUTTON, SetupElement.ASSIGN_SWITCH_CONTENT],
  },

  [SetupPageId.ASSIGN_PREVIOUS]: {
    titleId: 'switchAccessSetupAssignPreviousTitle',
    visibleElements:
        [SetupElement.PREVIOUS_BUTTON, SetupElement.ASSIGN_SWITCH_CONTENT],
  },

  [SetupPageId.CLOSING]: {
    titleId: 'switchAccessSetupClosingTitle',
    visibleElements: [
      SetupElement.DONE_BUTTON,
      SetupElement.START_OVER_BUTTON,
      SetupElement.CLOSING_CONTENT,
    ],
  },
};

export interface SettingsSwitchAccessSetupGuideDialogElement {
  $: {
    chooseSwitchCount: HTMLElement,
    closingInstructions: HTMLElement,
    titleText: HTMLElement,
    switchAccessSetupGuideDialog: CrDialogElement,
    switchCountGroup: CrRadioGroupElement,
  };
}

const SettingsSwitchAccessSetupGuideDialogElementBase =
    PrefsMixin(I18nMixin(PolymerElement));

export class SettingsSwitchAccessSetupGuideDialogElement extends
    SettingsSwitchAccessSetupGuideDialogElementBase {
  static get is() {
    return 'settings-switch-access-setup-guide-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      autoScanSpeedRangeMs_: {
        type: Array,
        value: [],
      },

      currentPageId_: {
        type: Number,
        value: SetupPageId.INTRO,
      },

      /**
       * A number formatter, to display values with exactly 1 digit after the
       * decimal (that has been internationalized properly).
       */
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

      switchCount_: {
        type: Number,
        value: 1,
      },

      switchToAssign_: {
        type: String,
        value: null,
      },
    };
  }

  static get observers() {
    return [
      `onSwitchAssignmentMaybeChanged_(
          prefs.settings.a11y.switch_access.next.*,
          prefs.settings.a11y.switch_access.previous.*,
          prefs.settings.a11y.switch_access.select.*)`,
    ];
  }

  private autoScanSpeedRangeMs_: SliderTick[];
  private currentPageId_: number;
  private formatter_: Intl.NumberFormat;
  private maxScanSpeedLabelSec_: string;
  private maxScanSpeedMs_: number;
  private minScanSpeedLabelSec_: string;
  private minScanSpeedMs_: number;
  private switchCount_: number;
  private switchToAssign_: SwitchAccessCommand|null;

  constructor() {
    super();

    this.maxScanSpeedLabelSec_ =
        this.scanSpeedStringInSec_(this.maxScanSpeedMs_);
    this.minScanSpeedLabelSec_ =
        this.scanSpeedStringInSec_(this.minScanSpeedMs_);
    this.autoScanSpeedRangeMs_ =
        this.ticksWithLabelsInSec_(AUTO_SCAN_SPEED_RANGE_MS);
  }

  override connectedCallback(): void {
    super.connectedCallback();

    SwitchAccessSubpageBrowserProxyImpl.getInstance()
        .notifySwitchAccessSetupGuideAttached();
  }

  override ready(): void {
    super.ready();

    this.addEventListener('exit-pane', this.onSwitchAssignmentMaybeChanged_);

    // Reset all switch assignments.
    for (const pref of Object.values(actionToPref)) {
      chrome.settingsPrivate.setPref(pref, {});
    }

    // Reset auto-scan.
    chrome.settingsPrivate.setPref(
        AUTO_SCAN_SPEED_PREF, DEFAULT_AUTO_SCAN_SPEED_MS);
    chrome.settingsPrivate.setPref(
        AUTO_SCAN_KEYBOARD_SPEED_PREF, DEFAULT_AUTO_SCAN_SPEED_MS);
    chrome.settingsPrivate.setPref(AUTO_SCAN_ENABLED_PREF, false);
  }

  private loadPage_(id: SetupPageId): void {
    this.addOrRemoveAssignmentPane_(id);

    const newPage = castExists(SetupPageList[id]);
    this.$.titleText.textContent = this.i18n(newPage.titleId);

    for (const elementId of Object.values(SetupElement)) {
      this.shadowRoot!.getElementById(elementId)!.hidden =
          !newPage.visibleElements.includes(elementId);
    }

    this.currentPageId_ = id;
  }

  /**
   * The assignment pane prevents Switch Access from receiving key events when
   * it is attached, which disables the user's navigational control. Therefore,
   * we add the assignment pane only when it's about to be displayed, and remove
   * it as soon as it's complete.
   */
  private addOrRemoveAssignmentPane_(id: SetupPageId): void {
    let action: SwitchAccessCommand|undefined;
    switch (id) {
      case SetupPageId.ASSIGN_SELECT:
        action = SwitchAccessCommand.SELECT;
        break;
      case SetupPageId.ASSIGN_NEXT:
        action = SwitchAccessCommand.NEXT;
        break;
      case SetupPageId.ASSIGN_PREVIOUS:
        action = SwitchAccessCommand.PREVIOUS;
    }

    if (action) {
      this.initializeAssignmentPane_(action);
    } else {
      this.removeAssignmentPaneIfPresent_();
    }
  }

  private initializeAssignmentPane_(action: SwitchAccessCommand): void {
    this.removeAssignmentPaneIfPresent_();

    this.switchToAssign_ = action;

    const assignmentPane =
        document.createElement('settings-switch-access-action-assignment-pane');
    assignmentPane.action = action;
    assignmentPane.context = AssignmentContext.SETUP_GUIDE;
    this.assignmentContentsElement.appendChild(assignmentPane);
  }

  private removeAssignmentPaneIfPresent_(): void {
    if (this.assignmentContentsElement.firstChild) {
      this.assignmentContentsElement.removeChild(
          this.assignmentContentsElement.firstChild);
    }

    this.switchToAssign_ = null;
  }

  /**
   * Determines what page is shown next, from the current page ID and other
   * state.
   */
  private getNextPageId_(): SetupPageId {
    switch (this.currentPageId_) {
      case SetupPageId.INTRO:
        return SetupPageId.ASSIGN_SELECT;
      case SetupPageId.ASSIGN_SELECT:
        return SetupPageId.AUTO_SCAN_ENABLED;
      case SetupPageId.AUTO_SCAN_ENABLED:
        return SetupPageId.CHOOSE_SWITCH_COUNT;
      case SetupPageId.CHOOSE_SWITCH_COUNT:
        if (this.switchCount_ === 3 || this.switchCount_ === 2) {
          return SetupPageId.ASSIGN_NEXT;
        } else {
          return SetupPageId.AUTO_SCAN_SPEED;
        }
      case SetupPageId.ASSIGN_NEXT:
        if (this.switchCount_ === 3) {
          return SetupPageId.ASSIGN_PREVIOUS;
        } else {
          return SetupPageId.CLOSING;
        }
      case SetupPageId.ASSIGN_PREVIOUS:
      case SetupPageId.AUTO_SCAN_SPEED:
      default:
        return SetupPageId.CLOSING;
    }
  }

  /**
   * Returns what page was shown previously from the current page ID.
   */
  private getPreviousPageId_(): SetupPageId {
    switch (this.currentPageId_) {
      case SetupPageId.CLOSING:
        if (this.switchCount_ === 3) {
          return SetupPageId.ASSIGN_PREVIOUS;
        } else if (this.switchCount_ === 2) {
          return SetupPageId.ASSIGN_NEXT;
        } else {
          return SetupPageId.AUTO_SCAN_SPEED;
        }
      case SetupPageId.ASSIGN_PREVIOUS:
        return SetupPageId.ASSIGN_NEXT;
      case SetupPageId.ASSIGN_NEXT:
      case SetupPageId.AUTO_SCAN_SPEED:
        return SetupPageId.CHOOSE_SWITCH_COUNT;
      case SetupPageId.CHOOSE_SWITCH_COUNT:
        return SetupPageId.AUTO_SCAN_ENABLED;
      case SetupPageId.AUTO_SCAN_ENABLED:
        return SetupPageId.ASSIGN_SELECT;
      case SetupPageId.ASSIGN_SELECT:
      default:
        return SetupPageId.INTRO;
    }
  }

  private onExitClick_(): void {
    this.$.switchAccessSetupGuideDialog.close();
  }

  private onStartOverClick_(): void {
    this.loadPage_(SetupPageId.INTRO);
  }

  private onNextClick_(): void {
    this.loadPage_(this.getNextPageId_());

    // Enable auto-scan when we reach that page of the setup guide.
    if (this.currentPageId_ === SetupPageId.AUTO_SCAN_ENABLED) {
      chrome.settingsPrivate.setPref(AUTO_SCAN_ENABLED_PREF, true);
    }

    // Disable auto-scan once the user has selected two or more switches.
    if (this.currentPageId_ === SetupPageId.ASSIGN_NEXT) {
      chrome.settingsPrivate.setPref(AUTO_SCAN_ENABLED_PREF, false);
    }

    if (this.currentPageId_ === SetupPageId.CLOSING) {
      if (this.switchCount_ >= 2) {
        this.$.closingInstructions.textContent =
            this.i18n('switchAccessSetupClosingManualScanInstructions');
      }
    }
  }

  private onPreviousClick_(): void {
    // Disable auto-scan when the user reverses to before it was enabled.
    if (this.currentPageId_ === SetupPageId.AUTO_SCAN_ENABLED) {
      chrome.settingsPrivate.setPref(AUTO_SCAN_ENABLED_PREF, false);
    }

    this.loadPage_(this.getPreviousPageId_());
  }

  private onBluetoothClick_(): void {
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES);
    this.$.switchAccessSetupGuideDialog.close();
  }

  private onAutoScanSpeedFaster_(): void {
    const pref = this.getPref<number>(AUTO_SCAN_SPEED_PREF);
    const currentValue = pref.value;
    // Find the first element in the array that is equal to, or smaller than,
    // the current value. Since AUTO_SCAN_SPEED_RANGE_MS is sorted largest to
    // smallest, this gives us a guarantee that we are within one step (100ms)
    // of the value at the provided index.
    const index =
        AUTO_SCAN_SPEED_RANGE_MS.findIndex(elem => elem <= currentValue);
    if (index === -1 || index === AUTO_SCAN_SPEED_RANGE_MS.length - 1) {
      return;
    }
    chrome.settingsPrivate.setPref(
        AUTO_SCAN_SPEED_PREF, AUTO_SCAN_SPEED_RANGE_MS[index + 1]);
  }

  private onAutoScanSpeedSlower_(): void {
    const pref = this.getPref<number>(AUTO_SCAN_SPEED_PREF);
    const currentValue = pref.value;
    // Find the first element in the array that is equal to, or smaller than,
    // the current value. Since AUTO_SCAN_SPEED_RANGE_MS is sorted largest to
    // smallest, this gives us a guarantee that we are within one step (100ms)
    // of the value at the provided index.
    const index =
        AUTO_SCAN_SPEED_RANGE_MS.findIndex(elem => elem <= currentValue);
    if (index <= 0) {
      return;
    }
    chrome.settingsPrivate.setPref(
        AUTO_SCAN_SPEED_PREF, AUTO_SCAN_SPEED_RANGE_MS[index - 1]);
  }

  private onSwitchCountChanged_(): void {
    const selected = this.$.switchCountGroup.selected;
    if (selected === 'one-switch') {
      this.switchCount_ = 1;
    } else if (selected === 'two-switches') {
      this.switchCount_ = 2;
    } else if (selected === 'three-switches') {
      this.switchCount_ = 3;
    }
  }

  private onSwitchAssignmentMaybeChanged_(): void {
    if (!this.assignmentContentsElement ||
        !this.assignmentContentsElement.firstChild) {
      return;
    }

    const currentAction = (this.assignmentContentsElement.firstChild as
                           SettingsSwitchAccessActionAssignmentPaneElement)
                              .action;
    const pref =
        this.getPref<Record<string, string[]>>(actionToPref[currentAction]);
    const hasSwitchAssigned = Object.keys(pref.value).length > 0;

    if (hasSwitchAssigned) {
      this.onNextClick_();
    } else {
      this.initializeAssignmentPane_(currentAction);
    }
  }

  private scanSpeedStringInSec_(scanSpeedValueMs: number): string {
    const scanSpeedValueSec = scanSpeedValueMs / 1000;
    return this.i18n(
        'durationInSeconds', this.formatter_.format(scanSpeedValueSec));
  }

  private ticksWithLabelsInSec_(ticksInMs: number[]): SliderTick[] {
    // Dividing by 1000 to convert milliseconds to seconds for the label.
    return ticksInMs.map(
        x => ({label: `${this.scanSpeedStringInSec_(x)}`, value: x}));
  }

  private get assignmentContentsElement(): HTMLElement {
    return castExists(
        this.shadowRoot!.getElementById(SetupElement.ASSIGN_SWITCH_CONTENT)!
            .querySelector('.sa-setup-contents'));
  }

  private getAssignSwitchIllo_(): string {
    switch (this.switchToAssign_) {
      case SwitchAccessCommand.SELECT:
        return 'os-settings-illo:switch-access-setup-guide-assign-select';
      case SwitchAccessCommand.NEXT:
        return 'os-settings-illo:switch-access-setup-guide-assign-next';
      case SwitchAccessCommand.PREVIOUS:
        return 'os-settings-illo:switch-access-setup-guide-assign-previous';
      default:
        return '';
    }
  }

  private getSwitchCountIllo_(): string {
    switch (this.switchCount_) {
      case 1:
        return 'os-settings-illo:switch-access-setup-guide-choose-1-switch';
      case 2:
        return 'os-settings-illo:switch-access-setup-guide-choose-2-switches';
      case 3:
        return 'os-settings-illo:switch-access-setup-guide-choose-3-switches';
      default:
        return '';
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-switch-access-setup-guide-dialog':
        SettingsSwitchAccessSetupGuideDialogElement;
  }
}

customElements.define(
    SettingsSwitchAccessSetupGuideDialogElement.is,
    SettingsSwitchAccessSetupGuideDialogElement);
