// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component walks a user through the flow of assigning a
 * switch key to a command/action. Note that command and action are used
 * interchangeably with command used internally and action used for user-facing
 * UI.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import '../os_settings_icons.html.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './switch_access_action_assignment_pane.html.js';
import {actionToPref, AssignmentContext, SwitchAccessCommand, SwitchAccessDeviceType} from './switch_access_constants.js';
import {SwitchAccessSubpageBrowserProxy, SwitchAccessSubpageBrowserProxyImpl} from './switch_access_subpage_browser_proxy.js';
import {KeyAssignment, SwitchAccessAssignmentsChangedValue} from './switch_access_types.js';

/**
 * Different states of the assignment flow dictating which overall view should
 * be shown.
 */
enum AssignmentState {
  WAIT_FOR_CONFIRMATION_REMOVAL = 0,
  WAIT_FOR_CONFIRMATION = 1,
  WAIT_FOR_KEY = 2,
  WARN_ALREADY_ASSIGNED_ACTION = 3,
  WARN_CANNOT_REMOVE_LAST_SELECT_SWITCH = 4,
  WARN_NOT_CONFIRMED_REMOVAL = 5,
  WARN_NOT_CONFIRMED = 6,
  WARN_UNRECOGNIZED_KEY = 7,
}

/**
 * Various icons representing the state of a given key assignment.
 */
enum AssignmentIcon {
  ASSIGNED = 'assigned',
  ADD_ASSIGNMENT = 'add-assignment',
  REMOVE_ASSIGNMENT = 'remove-assignment',
}

enum AssignmentResponse {
  EXIT = 'switchAccessActionAssignmentExitResponse',
  CONTINUE = 'switchAccessActionAssignmentContinueResponse',
  TRY_AGAIN = 'switchAccessActionAssignmentTryAgainResponse',
}

interface OnKeyWebUiEvent {
  key: string;
  keyCode: number;
  device: SwitchAccessDeviceType;
}

/**
 * Mapping of a stringified key code to a list of Switch Access device types
 * for that key code.
 */
interface SwitchAccessKeyAssignmentInfoMapping {
  [key: string]: SwitchAccessDeviceType[];
}

const SettingsSwitchAccessActionAssignmentPaneElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class SettingsSwitchAccessActionAssignmentPaneElement extends
    SettingsSwitchAccessActionAssignmentPaneElementBase {
  static get is() {
    return 'settings-switch-access-action-assignment-pane';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Specify which switch action this pane handles.
       */
      action: {
        type: String,
      },

      /**
       * Specify the context this pane is located in.
       */
      context: String,

      /**
       * Enable the html template to use AssignmentState.
       */
      assignmentState_: {
        type: Number,
        value: AssignmentState.WAIT_FOR_KEY,
      },

      /**
       * Assignments for the current action.
       */
      assignments_: {
        type: Array,
        value: [],
      },

      /**
       * A dictionary containing all Switch Access key codes (mapped from
       * actions).
       */
      keyCodes_: {
        type: Object,
        value: {select: {}, next: {}, previous: {}},
      },

      /**
       * User prompt text shown at the top of the pane.
       */
      promptText_: {
        type: String,
        computed: 'computePromptText_(assignmentState_, assignments_)',
      },

      /**
       * Error text shown on the pane with error symbol. Hidden if  blank.
       */
      errorText_: {
        type: String,
        computed: 'computeErrorText_(assignmentState_)',
      },

      /**
       * The label indicating there are no switches.
       */
      noSwitchesLabel_: String,

      alreadyAssignedAction_: String,

      currentKey_: String,

      currentKeyCode_: {
        type: Number,
        value: null,
      },

      currentDeviceType_: String,
    };
  }

  action: SwitchAccessCommand;
  context: AssignmentContext;
  private alreadyAssignedAction_: SwitchAccessCommand;
  private assignmentState_: AssignmentState;
  private assignments_: KeyAssignment[];
  private currentDeviceType_: SwitchAccessDeviceType;
  private currentKey_: string;
  private currentKeyCode_: number|null;
  private errorText_: string;
  private keyCodes_:
      {[key in SwitchAccessCommand]: SwitchAccessKeyAssignmentInfoMapping};
  private noSwitchesLabel_: string;
  private promptText_: string;
  private switchAccessBrowserProxy_: SwitchAccessSubpageBrowserProxy;

  constructor() {
    super();

    this.switchAccessBrowserProxy_ =
        SwitchAccessSubpageBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    // Save all existing prefs.
    for (const action in actionToPref) {
      chrome.settingsPrivate
          .getPref(actionToPref[action as SwitchAccessCommand])
          .then((pref: {value: SwitchAccessKeyAssignmentInfoMapping}) => {
            this.keyCodes_[action as SwitchAccessCommand] = pref.value;
          });
    }

    this.addWebUiListener(
        'switch-access-got-key-press-for-assignment',
        (event: OnKeyWebUiEvent) => this.onKeyDown_(event));
    this.addWebUiListener(
        'switch-access-assignments-changed',
        (value: SwitchAccessAssignmentsChangedValue) =>
            this.onAssignmentsChanged_(value));
    this.switchAccessBrowserProxy_
        .notifySwitchAccessActionAssignmentPaneActive();

    if (this.context === AssignmentContext.SETUP_GUIDE) {
      this.noSwitchesLabel_ = this.i18n('noSwitchesAssignedSetupGuide');
    } else {
      this.noSwitchesLabel_ = this.i18n('noSwitchesAssigned');
    }
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    this.switchAccessBrowserProxy_
        .notifySwitchAccessActionAssignmentPaneInactive();

    // Restore everything before we close.
    for (const action in actionToPref) {
      chrome.settingsPrivate.setPref(
          actionToPref[action as SwitchAccessCommand],
          this.keyCodes_[action as SwitchAccessCommand]);
    }
  }

  private onKeyDown_(event: OnKeyWebUiEvent): void {
    switch (this.assignmentState_) {
      case AssignmentState.WAIT_FOR_KEY:
        this.handleKeyEventInWaitForKey_(event);
        break;
      case AssignmentState.WAIT_FOR_CONFIRMATION:
        this.handleKeyEventInWaitForConfirmation_(event);
        break;
      case AssignmentState.WAIT_FOR_CONFIRMATION_REMOVAL:
        this.handleKeyEventInWaitForConfirmationRemoval_(event);
        break;
      case AssignmentState.WARN_NOT_CONFIRMED:
      case AssignmentState.WARN_NOT_CONFIRMED_REMOVAL:
      case AssignmentState.WARN_ALREADY_ASSIGNED_ACTION:
      case AssignmentState.WARN_UNRECOGNIZED_KEY:
      case AssignmentState.WARN_CANNOT_REMOVE_LAST_SELECT_SWITCH:
        this.fireExitPane_();
        break;
    }
  }

  /**
   * In this state, the pane waits for the user to press the initial switch key
   * to be assigned.
   */
  private handleKeyEventInWaitForKey_(event: OnKeyWebUiEvent): void {
    this.currentKeyCode_ = event.keyCode;
    this.currentKey_ = event.key;
    this.currentDeviceType_ = event.device;

    if (!this.currentKey_) {
      this.assignmentState_ = AssignmentState.WARN_UNRECOGNIZED_KEY;
      return;
    }

    // Check for pre-existing assignments in actions other than the current one.
    for (const action of Object.values(SwitchAccessCommand)) {
      if (!this.keyCodes_[action][event.keyCode] ||
          !this.keyCodes_[action][event.keyCode].includes(
              this.currentDeviceType_)) {
        continue;
      }

      if (action === this.action) {
        if (action === SwitchAccessCommand.SELECT &&
            this.assignments_.length === 1) {
          this.assignmentState_ =
              AssignmentState.WARN_CANNOT_REMOVE_LAST_SELECT_SWITCH;
          return;
        }
        this.assignmentState_ = AssignmentState.WAIT_FOR_CONFIRMATION_REMOVAL;
      } else {
        this.alreadyAssignedAction_ = action;
        this.assignmentState_ = AssignmentState.WARN_ALREADY_ASSIGNED_ACTION;
      }
      return;
    }
    this.assignmentState_ = AssignmentState.WAIT_FOR_CONFIRMATION;
    this.push(
        'assignments_',
        {key: this.currentKey_, device: this.currentDeviceType_});
  }

  /**
   * In this state, the user has pressed the initial switch key, which is not
   * already assigned to any action. The pane waits for the user to press the
   * switch key again to confirm assignment.
   */
  private handleKeyEventInWaitForConfirmation_(event: OnKeyWebUiEvent): void {
    if (this.currentKeyCode_ !== event.keyCode ||
        this.currentDeviceType_ !== event.device) {
      this.assignmentState_ = AssignmentState.WARN_NOT_CONFIRMED;
      return;
    }

    // Save the key to |this.keyCodes_| for inclusion into prefs later.
    const keyAssignmentInfoMapping = this.keyCodes_[this.action];
    if (!keyAssignmentInfoMapping) {
      throw new Error('Expected valid pref for action: ' + this.action);
    }
    let devices = keyAssignmentInfoMapping[this.currentKeyCode_];
    if (!devices) {
      // |this.currentKeyCode_| was not set as a switch key for |this.action|
      // before.
      devices = [];
      keyAssignmentInfoMapping[this.currentKeyCode_] = devices;
    }
    if (!devices.includes(event.device)) {
      // A new device for the current key code has been added.
      devices.push(event.device);
    }

    this.fireExitPane_();
  }

  /**
   * In this state, the user has pressed the initial switch key, which is
   * already assigned to the current action. The pane waits for the user to
   * press the switch key again to confirm removal.
   */
  private handleKeyEventInWaitForConfirmationRemoval_(event: OnKeyWebUiEvent):
      void {
    if (this.currentKeyCode_ !== event.keyCode ||
        this.currentDeviceType_ !== event.device) {
      this.assignmentState_ = AssignmentState.WARN_NOT_CONFIRMED_REMOVAL;
      return;
    }

    // Remove this device type for this key code.
    const devices = this.keyCodes_[this.action][this.currentKeyCode_];
    devices.splice(devices.indexOf(event.device), 1);
    if (!devices.length) {
      delete this.keyCodes_[this.action][this.currentKeyCode_];
    }

    this.fireExitPane_();
  }

  private onAssignmentsChanged_(value: SwitchAccessAssignmentsChangedValue):
      void {
    this.assignments_ = value[this.action];
  }

  /**
   * Fires an 'exit-pane' event.
   */
  private fireExitPane_(): void {
    const exitPaneEvent =
        new CustomEvent('exit-pane', {bubbles: true, composed: true});
    this.dispatchEvent(exitPaneEvent);
  }

  private getLabelForAction_(action: SwitchAccessCommand): string {
    switch (action) {
      case SwitchAccessCommand.SELECT:
        return this.i18n('assignSelectSwitchLabel');
      case SwitchAccessCommand.NEXT:
        return this.i18n('assignNextSwitchLabel');
      case SwitchAccessCommand.PREVIOUS:
        return this.i18n('assignPreviousSwitchLabel');
      default:
        assertNotReached();
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
   * Returns the image to use for the assignment's icon. The value must match
   * one of iron-icon's os-settings:(*) icon names.
   */
  private computeIcon_(assignment: KeyAssignment): AssignmentIcon {
    if (assignment.key !== this.currentKey_ ||
        assignment.device !== this.currentDeviceType_) {
      return AssignmentIcon.ASSIGNED;
    }

    switch (this.assignmentState_) {
      case AssignmentState.WAIT_FOR_KEY:
      case AssignmentState.WARN_ALREADY_ASSIGNED_ACTION:
      case AssignmentState.WARN_UNRECOGNIZED_KEY:
      case AssignmentState.WARN_CANNOT_REMOVE_LAST_SELECT_SWITCH:
        return AssignmentIcon.ASSIGNED;
      case AssignmentState.WAIT_FOR_CONFIRMATION:
      case AssignmentState.WARN_NOT_CONFIRMED:
        return AssignmentIcon.ADD_ASSIGNMENT;
      case AssignmentState.WAIT_FOR_CONFIRMATION_REMOVAL:
      case AssignmentState.WARN_NOT_CONFIRMED_REMOVAL:
        return AssignmentIcon.REMOVE_ASSIGNMENT;
      default:
        assertNotReached('Invalid assignment state.');
    }
  }

  /**
   * Returns the icon label describing the icon for the specified assignment.
   */
  private computeIconLabel_(assignment: KeyAssignment): string {
    const icon = this.computeIcon_(assignment);
    switch (icon) {
      case AssignmentIcon.ASSIGNED:
        return this.i18n('switchAccessActionAssignmentAssignedIconLabel');
      case AssignmentIcon.ADD_ASSIGNMENT:
        return this.i18n('switchAccessActionAssignmentAddAssignmentIconLabel');
      case AssignmentIcon.REMOVE_ASSIGNMENT:
        return this.i18n(
            'switchAccessActionAssignmentRemoveAssignmentIconLabel');
      default:
        assertNotReached('Invalid assignment icon.');
    }
  }

  private computePromptText_(
      assignmentState: AssignmentState, assignments: KeyAssignment[]): string {
    let response: AssignmentResponse;
    switch (assignmentState) {
      case AssignmentState.WAIT_FOR_KEY:
      case AssignmentState.WARN_ALREADY_ASSIGNED_ACTION:
      case AssignmentState.WARN_UNRECOGNIZED_KEY:
      case AssignmentState.WARN_CANNOT_REMOVE_LAST_SELECT_SWITCH:
        if (!assignments.length) {
          if (this.context === AssignmentContext.SETUP_GUIDE) {
            return this.i18n(
                'switchAccessActionAssignmentWaitForKeyPromptNoSwitchesSetupGuide',
                this.getLabelForAction_(this.action));
          }
          return this.i18n(
              'switchAccessActionAssignmentWaitForKeyPromptNoSwitches',
              this.getLabelForAction_(this.action));
        }
        return this.i18n(
            'switchAccessActionAssignmentWaitForKeyPromptAtLeastOneSwitch');
      case AssignmentState.WAIT_FOR_CONFIRMATION:
      case AssignmentState.WARN_NOT_CONFIRMED:
        response = this.context === AssignmentContext.SETUP_GUIDE ?
            AssignmentResponse.CONTINUE :
            AssignmentResponse.EXIT;
        return this.i18n(
            'switchAccessActionAssignmentWaitForConfirmationPrompt',
            this.currentKey_, this.i18n(response));
      case AssignmentState.WAIT_FOR_CONFIRMATION_REMOVAL:
      case AssignmentState.WARN_NOT_CONFIRMED_REMOVAL:
        response = this.context === AssignmentContext.SETUP_GUIDE ?
            AssignmentResponse.TRY_AGAIN :
            AssignmentResponse.EXIT;
        return this.i18n(
            'switchAccessActionAssignmentWaitForConfirmationRemovalPrompt',
            this.currentKey_, this.i18n(response));
      default:
        assertNotReached('Invalid assignment state.');
    }
  }

  private computeErrorText_(assignmentState: AssignmentState): string {
    let response = this.context === AssignmentContext.SETUP_GUIDE ?
        AssignmentResponse.TRY_AGAIN :
        AssignmentResponse.EXIT;
    switch (assignmentState) {
      case AssignmentState.WARN_NOT_CONFIRMED:
      case AssignmentState.WARN_NOT_CONFIRMED_REMOVAL:
        return this.i18n(
            'switchAccessActionAssignmentWarnNotConfirmedPrompt',
            this.i18n(response));
      case AssignmentState.WARN_ALREADY_ASSIGNED_ACTION:
        return this.i18n(
            'switchAccessActionAssignmentWarnAlreadyAssignedActionPrompt',
            this.currentKey_,
            this.getLabelForAction_(this.alreadyAssignedAction_),
            this.i18n(response));
      case AssignmentState.WARN_UNRECOGNIZED_KEY:
        return this.i18n(
            'switchAccessActionAssignmentWarnUnrecognizedKeyPrompt',
            this.i18n(response));
      case AssignmentState.WARN_CANNOT_REMOVE_LAST_SELECT_SWITCH:
        if (this.context === AssignmentContext.SETUP_GUIDE) {
          response = AssignmentResponse.CONTINUE;
        }
        return this.i18n(
            'switchAccessActionAssignmentWarnCannotRemoveLastSelectSwitch',
            this.i18n(response));
      case AssignmentState.WAIT_FOR_KEY:
      case AssignmentState.WAIT_FOR_CONFIRMATION:
      case AssignmentState.WAIT_FOR_CONFIRMATION_REMOVAL:
        return '';
      default:
        assertNotReached('Invalid assignment state.');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-switch-access-action-assignment-pane':
        SettingsSwitchAccessActionAssignmentPaneElement;
  }
}

customElements.define(
    SettingsSwitchAccessActionAssignmentPaneElement.is,
    SettingsSwitchAccessActionAssignmentPaneElement);
