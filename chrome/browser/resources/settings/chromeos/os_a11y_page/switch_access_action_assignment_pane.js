// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component walks a user through the flow of assigning a
 * switch key to a command/action. Note that command and action are used
 * interchangeably with command used internally and action used for user-facing
 * UI.
 */

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '../os_icons.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {actionToPref, AssignmentContext, AUTO_SCAN_SPEED_RANGE_MS, SwitchAccessCommand, SwitchAccessDeviceType} from './switch_access_constants.js';
import {SwitchAccessSubpageBrowserProxy, SwitchAccessSubpageBrowserProxyImpl} from './switch_access_subpage_browser_proxy.js';

/**
 * Different states of the assignment flow dictating which overall view should
 * be shown.
 * @enum {number}
 */
export const AssignmentState = {
  WAIT_FOR_CONFIRMATION_REMOVAL: 0,
  WAIT_FOR_CONFIRMATION: 1,
  WAIT_FOR_KEY: 2,
  WARN_ALREADY_ASSIGNED_ACTION: 3,
  WARN_CANNOT_REMOVE_LAST_SELECT_SWITCH: 4,
  WARN_NOT_CONFIRMED_REMOVAL: 5,
  WARN_NOT_CONFIRMED: 6,
  WARN_UNRECOGNIZED_KEY: 7,
};

/**
 * Various icons representing the state of a given key assignment.
 * @enum {string}
 */
export const AssignmentIcon = {
  ASSIGNED: 'assigned',
  ADD_ASSIGNMENT: 'add-assignment',
  REMOVE_ASSIGNMENT: 'remove-assignment',
};

/** @enum {string} */
const AssignmentResponse = {
  EXIT: 'switchAccessActionAssignmentExitResponse',
  CONTINUE: 'switchAccessActionAssignmentContinueResponse',
  TRY_AGAIN: 'switchAccessActionAssignmentTryAgainResponse',
};

/**
 * Mapping of a stringified key code to a list of Switch Access device types
 * for that key code.
 * @typedef {!Object<string, !Array<!SwitchAccessDeviceType>>}
 */
let SwitchAccessKeyAssignmentInfoMapping;

/**
 * @param {!SwitchAccessDeviceType} deviceType
 * @return {string}
 */
export function getLabelForDeviceType(deviceType) {
  switch (deviceType) {
    case SwitchAccessDeviceType.INTERNAL:
      return I18nBehavior.i18nAdvanced(
          'switchAccessInternalDeviceTypeLabel', {});
    case SwitchAccessDeviceType.USB:
      return I18nBehavior.i18nAdvanced('switchAccessUsbDeviceTypeLabel', {});
    case SwitchAccessDeviceType.BLUETOOTH:
      return I18nBehavior.i18nAdvanced(
          'switchAccessBluetoothDeviceTypeLabel', {});
    case SwitchAccessDeviceType.UNKNOWN:
      return I18nBehavior.i18nAdvanced(
          'switchAccessUnknownDeviceTypeLabel', {});
  }
  throw new Error('Invalid device type.');
}

/**
 * Converts assignment object to pretty-formatted label.
 * E.g. {key: 'Escape', device: 'usb'} -> 'Escape (USB)'
 * @param {{key: string, device: !SwitchAccessDeviceType}} assignment
 * @return {string}
 */
export function getLabelForAssignment(assignment) {
  return I18nBehavior.i18nAdvanced('switchAndDeviceType', {
    substitutions: [assignment.key, getLabelForDeviceType(assignment.device)]
  });
}

// TODO(crbug.com/1222452): Convert to use Polymer's class based syntax (e.g.
// https://crrev.com/c/2808034).
Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-switch-access-action-assignment-pane',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Specify which switch action this pane handles.
     * @type {SwitchAccessCommand}
     */
    action: {
      type: String,
    },

    /**
     * Specify the context this pane is located in.
     * @type {AssignmentContext}
     */
    context: String,

    /**
     * Enable the html template to use AssignmentState.
     * @private {AssignmentState}
     */
    assignmentState_: {
      type: Number,
      value: AssignmentState.WAIT_FOR_KEY,
    },

    /**
     * Assignments for the current action.
     * @private {!Array<{key: string, device: !SwitchAccessDeviceType}>}
     */
    assignments_: {
      type: Array,
      value: [],
    },

    /**
     * A dictionary containing all Switch Access key codes (mapped from
     * actions).
     * @private {{
     *         select: SwitchAccessKeyAssignmentInfoMapping,
     *         next: SwitchAccessKeyAssignmentInfoMapping,
     *         previous: SwitchAccessKeyAssignmentInfoMapping
     * }}
     */
    keyCodes_: {
      type: Object,
      value: {select: {}, next: {}, previous: {}},
    },

    /**
     * User prompt text shown at the top of the pane.
     * @private
     */
    promptText_: {
      type: String,
      computed: 'computePromptText_(assignmentState_, assignments_)',
    },

    /**
     * Error text shown on the pane with error symbol. Hidden if blank.
     * @private
     */
    errorText_: {
      type: String,
      computed: 'computeErrorText_(assignmentState_)',
    },

    /**
     * The label indicating there are no switches.
     * @private
     */
    noSwitchesLabel_: String,

    /** @private {!SwitchAccessCommand} */
    alreadyAssignedAction_: String,

    /** @private {!string} */
    currentKey_: String,

    /** @private {?number} */
    currentKeyCode_: {
      type: Number,
      value: null,
    },

    /** @private {!SwitchAccessDeviceType} */
    currentDeviceType_: String,
  },

  /** @private {?SwitchAccessSubpageBrowserProxy} */
  switchAccessBrowserProxy_: null,

  /** @override */
  created() {
    this.switchAccessBrowserProxy_ =
        SwitchAccessSubpageBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    // Save all existing prefs.
    for (const action in actionToPref) {
      chrome.settingsPrivate.getPref(actionToPref[action], (pref) => {
        this.keyCodes_[action] = pref.value;
      });
    }

    this.addWebUIListener(
        'switch-access-got-key-press-for-assignment',
        event => this.onKeyDown_(event));
    this.addWebUIListener(
        'switch-access-assignments-changed',
        value => this.onAssignmentsChanged_(value));
    this.switchAccessBrowserProxy_
        .notifySwitchAccessActionAssignmentPaneActive();

    if (this.context === AssignmentContext.SETUP_GUIDE) {
      this.noSwitchesLabel_ = this.i18n('noSwitchesAssignedSetupGuide');
    } else {
      this.noSwitchesLabel_ = this.i18n('noSwitchesAssigned');
    }
  },

  /** @override */
  detached() {
    this.switchAccessBrowserProxy_
        .notifySwitchAccessActionAssignmentPaneInactive();

    // Restore everything before we close.
    for (const action in actionToPref) {
      chrome.settingsPrivate.setPref(
          actionToPref[action], this.keyCodes_[action]);
    }
  },

  /**
   * @param {{key: string, keyCode: number}} event
   * @private
   */
  onKeyDown_(event) {
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
  },

  /**
   * In this state, the pane waits for the user to press the initial switch key
   * to be assigned.
   * @param {{key: string, keyCode: number}} event
   * @private
   */
  handleKeyEventInWaitForKey_(event) {
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
  },

  /**
   * In this state, the user has pressed the initial switch key, which is not
   * already assigned to any action. The pane waits for the user to press the
   * switch key again to confirm assignment.
   * @param {{key: string, keyCode: number}} event
   * @private
   */
  handleKeyEventInWaitForConfirmation_(event) {
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
  },

  /**
   * In this state, the user has pressed the initial switch key, which is
   * already assigned to the current action. The pane waits for the user to
   * press the switch key again to confirm removal.
   * @param {{key: string, keyCode: number}} event
   * @private
   */
  handleKeyEventInWaitForConfirmationRemoval_(event) {
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
  },

  /**
   * @param {!Object<SwitchAccessCommand, !Array<{key: string, device:
   *     !SwitchAccessDeviceType}>>} value
   * @private
   */
  onAssignmentsChanged_(value) {
    this.assignments_ = value[this.action];
  },

  /**
   * Fires an 'exit-pane' event.
   * @private
   */
  fireExitPane_() {
    this.fire('exit-pane');
  },

  /**
   * @param {SwitchAccessCommand} action
   * @return {string}
   * @private
   */
  getLabelForAction_(action) {
    switch (action) {
      case SwitchAccessCommand.SELECT:
        return this.i18n('assignSelectSwitchLabel');
      case SwitchAccessCommand.NEXT:
        return this.i18n('assignNextSwitchLabel');
      case SwitchAccessCommand.PREVIOUS:
        return this.i18n('assignPreviousSwitchLabel');
      default:
        return '';
    }
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
   * Returns the image to use for the assignment's icon. The value must match
   * one of iron-icon's os-settings:(*) icon names.
   * @param {{key: string, device: !SwitchAccessDeviceType}} assignment
   * @return {AssignmentIcon}
   * @private
   */
  computeIcon_(assignment) {
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
    }
    throw new Error('Invalid assignment state.');
  },

  /**
   * Returns the icon label describing the icon for the specified assignment.
   * @param {{key: string, device: !SwitchAccessDeviceType}} assignment
   * @return {string}
   * @private
   */
  computeIconLabel_(assignment) {
    const icon = this.computeIcon_(assignment);
    switch (icon) {
      case AssignmentIcon.ASSIGNED:
        return this.i18n('switchAccessActionAssignmentAssignedIconLabel');
      case AssignmentIcon.ADD_ASSIGNMENT:
        return this.i18n('switchAccessActionAssignmentAddAssignmentIconLabel');
      case AssignmentIcon.REMOVE_ASSIGNMENT:
        return this.i18n(
            'switchAccessActionAssignmentRemoveAssignmentIconLabel');
    }
    throw new Error('Invalid assignment icon.');
  },

  /**
   * @param {!AssignmentState} assignmentState
   * @param {!Array<{key: string, device: !SwitchAccessDeviceType}>} assignments
   * @return {string}
   * @private
   */
  computePromptText_(assignmentState, assignments) {
    let response;
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
    }
    throw new Error('Invalid assignment state.');
  },

  /**
   * @param {!AssignmentState} assignmentState
   * @return {string}
   * @private
   */
  computeErrorText_(assignmentState) {
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
    }
    throw new Error('Invalid assignment state.');
  },
});
