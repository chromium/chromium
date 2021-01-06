// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog walks a user through the flow of assign a switch
 * key to a command/action. Note that command and action are used
 * interchangeably with command used internally and action used for user-facing
 * UI.
 */

/**
 * Different states of the assignment flow dictating which overall dialogue view
 * should be shown.
 * @enum {number}
 */
/* #export */ const AssignmentState = {
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
 * Maps a action to its pref name.
 * @const {!Object<SwitchAccessCommand, string>}
 * @private
 */
/* #export */ const actionToPref = {
  select: 'settings.a11y.switch_access.select.device_key_codes',
  next: 'settings.a11y.switch_access.next.device_key_codes',
  previous: 'settings.a11y.switch_access.previous.device_key_codes'
};

/**
 * Possible device types for Switch Access.
 * @enum {string}
 * @private
 */
/* #export */ const SwitchAccessDeviceType = {
  INTERNAL: 'internal',
  USB: 'usb',
  BLUETOOTH: 'bluetooth',
  UNKNOWN: 'unknown'
};

/**
 * Various icons representing the state of a given key assignment.
 * @enum {string}
 */
/* #export */ const AssignmentIcon = {
  ASSIGNED: 'assigned',
  ADD_ASSIGNMENT: 'add-assignment',
  REMOVE_ASSIGNMENT: 'remove-assignment',
};

/**
 * @typedef {!Array<!Object<string, !Array<!SwitchAccessDeviceType>>>}
 */
let SwitchAccessKeyAssignmentInfoList;

Polymer({
  is: 'settings-switch-access-action-assignment-dialog',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Set by the main Switch Access subpage to specify which switch action this
     * dialog handles.
     * @type {SwitchAccessCommand}
     */
    action: {
      type: String,
    },

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
     * @private {Array<!{key: string, devices:
     *     !Array<!SwitchAccessDeviceType>}>}
     */
    assignments_: {
      type: Array,
      value: [],
    },

    /**
     * The localized action label.
     * @private {string}
     */
    dialogTitle_: {
      type: String,
      computed: 'getDialogTitleForAction_(action)',
    },

    /**
     * A dictionary containing all Switch Access key codes (mapped from
     * actions).
     * Each key code is another object, mapping a stringified key code to a list
     * of Switch Access device types for that key code.
     * @private {{
     *         select: SwitchAccessKeyAssignmentInfoList,
     *         next: SwitchAccessKeyAssignmentInfoList,
     *         previous: SwitchAccessKeyAssignmentInfoList
     * }}
     */
    keyCodes_: {
      type: Object,
      value: {select: {}, next: {}, previous: {}},
    },

    /**
     * User prompt text shown on the dialog.
     * @private {string}
     */
    promptText_: {
      type: String,
      computed: 'computePromptText_(assignmentState_, assignments_)',
    },

    /**
     * Error text shown on the dialog with error symbol. Hidden if blank.
     * @private {string}
     */
    errorText_: {
      type: String,
      computed: 'computeErrorText_(assignmentState_)',
    },

    /** @private {!SwitchAccessCommand} */
    alreadyAssignedAction_: String,

    /** @private {!string} */
    currentKey_: String,

    /** @private {!string} */
    unexpectedKey_: String,

    /** @private {?number} */
    currentKeyCode_: {
      type: Number,
      value: null,
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
  attached() {
    // Save all existing prefs.
    for (const action in actionToPref) {
      chrome.settingsPrivate.getPref(actionToPref[action], (pref) => {
        this.keyCodes_[action] = pref.value;
      });
    }

    this.addWebUIListener(
        'switch-access-got-key-press-for-assignment',
        this.onKeyDown_.bind(this));
    this.addWebUIListener(
        'switch-access-assignments-changed',
        this.onAssignmentsChanged_.bind(this));
    this.switchAccessBrowserProxy_
        .notifySwitchAccessActionAssignmentDialogAttached();
  },

  /** @override */
  detached() {
    this.switchAccessBrowserProxy_
        .notifySwitchAccessActionAssignmentDialogDetached();

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
        this.$.switchAccessActionAssignmentDialog.close();
        break;
    }
  },

  /**
   * In this state, the dialog waits for the user to press the initial switch
   * key to be assigned.
   * @param {{key: string, keyCode: number}} event
   * @private
   */
  handleKeyEventInWaitForKey_(event) {
    this.currentKeyCode_ = event.keyCode;
    this.currentKey_ = event.key;

    if (!this.currentKey_) {
      this.assignmentState_ = AssignmentState.WARN_UNRECOGNIZED_KEY;
      return;
    }

    // Check for pre-existing assignments in actions other than the current one.
    for (const action of Object.values(SwitchAccessCommand)) {
      if (!this.keyCodes_[action][event.keyCode]) {
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
    this.push('assignments_', this.currentKey_);
  },

  /**
   * In this state, the user has pressed the initial switch key, which is not
   * already assigned to any action. The dialog waits for the user to
   * press the switch key again to confirm assignment.
   * @param {{key: string, keyCode: number}} event
   * @private
   */
  handleKeyEventInWaitForConfirmation_(event) {
    if (this.currentKeyCode_ === event.keyCode) {
      // Confirmed.
      // TODO: resolve to specific device type once UI is hooked up;
      // |event.device| has the Switch Access device type.
      this.keyCodes_[this.action][this.currentKeyCode_] = [
        SwitchAccessDeviceType.INTERNAL, SwitchAccessDeviceType.USB,
        SwitchAccessDeviceType.BLUETOOTH
      ];
      this.$.switchAccessActionAssignmentDialog.close();
      return;
    }

    // Not confirmed.
    this.unexpectedKey_ = event.key;
    this.assignmentState_ = AssignmentState.WARN_NOT_CONFIRMED;
  },

  /**
   * In this state, the user has pressed the initial switch key, which is
   * already assigned to the current action. The dialog waits for the user to
   * press the switch key again to confirm removal.
   * @param {{key: string, keyCode: number}} event
   * @private
   */
  handleKeyEventInWaitForConfirmationRemoval_(event) {
    if (this.currentKeyCode_ !== event.keyCode) {
      this.unexpectedKey_ = event.key;
      this.assignmentState_ = AssignmentState.WARN_NOT_CONFIRMED_REMOVAL;
      return;
    }

    // Remove this key code.
    delete this.keyCodes_[this.action][this.currentKeyCode_];

    this.$.switchAccessActionAssignmentDialog.close();
  },

  /** @private */
  onExitClick_() {
    this.$.switchAccessActionAssignmentDialog.close();
  },

  /**
   * @param {!Object<SwitchAccessCommand, !Array<!{key: string, devices:
   *     !Array<SwitchAccessDeviceType>}>>} value
   * @private
   */
  onAssignmentsChanged_(value) {
    this.assignments_ = value[this.action];
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
   * @param {SwitchAccessCommand} action
   * @return {string}
   * @private
   */
  getDialogTitleForAction_(action) {
    return this.i18n(
        'switchAccessActionAssignmentDialogTitle',
        this.getLabelForAction_(action));
  },

  /**
   * Returns the image to use for the assignment's icon. The value must match
   * one of iron-icon's os-settings:(*) icon names.
   * @param {string} assignment
   * @return {AssignmentIcon}
   * @private
   */
  computeIcon_(assignment) {
    if (assignment !== this.currentKey_) {
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
   * @param {string} assignment
   * @return {string}
   * @private
   */
  computeIconLabel_(assignment) {
    const icon = this.computeIcon_(assignment);
    switch (icon) {
      case AssignmentIcon.ASSIGNED:
        return this.i18n('switchAccessActionAssignmentDialogAssignedIconLabel');
      case AssignmentIcon.ADD_ASSIGNMENT:
        return this.i18n(
            'switchAccessActionAssignmentDialogAddAssignmentIconLabel');
      case AssignmentIcon.REMOVE_ASSIGNMENT:
        return this.i18n(
            'switchAccessActionAssignmentDialogRemoveAssignmentIconLabel');
    }
    throw new Error('Invalid assignment icon.');
  },

  /**
   * @param {!AssignmentState} assignmentState
   * @param {!Array<string>} assignments
   * @return {string}
   * @private
   */
  computePromptText_(assignmentState, assignments) {
    switch (assignmentState) {
      case AssignmentState.WAIT_FOR_KEY:
      case AssignmentState.WARN_ALREADY_ASSIGNED_ACTION:
      case AssignmentState.WARN_UNRECOGNIZED_KEY:
      case AssignmentState.WARN_CANNOT_REMOVE_LAST_SELECT_SWITCH:
        if (!assignments.length) {
          return this.i18n(
              'switchAccessActionAssignmentDialogWaitForKeyPromptNoSwitches',
              this.getLabelForAction_(this.action));
        }
        return this.i18n(
            'switchAccessActionAssignmentDialogWaitForKeyPromptAtLeastOneSwitch');
      case AssignmentState.WAIT_FOR_CONFIRMATION:
      case AssignmentState.WARN_NOT_CONFIRMED:
        return this.i18n(
            'switchAccessActionAssignmentDialogWaitForConfirmationPrompt',
            this.currentKey_);
      case AssignmentState.WAIT_FOR_CONFIRMATION_REMOVAL:
      case AssignmentState.WARN_NOT_CONFIRMED_REMOVAL:
        return this.i18n(
            'switchAccessActionAssignmentDialogWaitForConfirmationRemovalPrompt',
            this.currentKey_);
    }
    throw new Error('Invalid assignment state.');
  },

  /**
   * @param {!AssignmentState} assignmentState
   * @return {string}
   * @private
   */
  computeErrorText_(assignmentState) {
    switch (assignmentState) {
      case AssignmentState.WARN_NOT_CONFIRMED:
      case AssignmentState.WARN_NOT_CONFIRMED_REMOVAL:
        return this.i18n(
            'switchAccessActionAssignmentDialogWarnNotConfirmedPrompt');
      case AssignmentState.WARN_ALREADY_ASSIGNED_ACTION:
        return this.i18n(
            'switchAccessActionAssignmentDialogWarnAlreadyAssignedActionPrompt',
            this.currentKey_,
            this.getLabelForAction_(this.alreadyAssignedAction_));
      case AssignmentState.WARN_UNRECOGNIZED_KEY:
        return this.i18n(
            'switchAccessActionAssignmentDialogWarnUnrecognizedKeyPrompt');
      case AssignmentState.WARN_CANNOT_REMOVE_LAST_SELECT_SWITCH:
        return this.i18n(
            'switchAccessActionAssignmentDialogWarnCannotRemoveLastSelectSwitch');
      case AssignmentState.WAIT_FOR_KEY:
      case AssignmentState.WAIT_FOR_CONFIRMATION:
      case AssignmentState.WAIT_FOR_CONFIRMATION_REMOVAL:
        return '';
    }
    throw new Error('Invalid assignment state.');
  },
});
