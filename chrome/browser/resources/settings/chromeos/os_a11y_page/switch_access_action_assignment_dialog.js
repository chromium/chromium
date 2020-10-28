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
  WAIT_FOR_KEY: 0,
  WAIT_FOR_CONFIRMATION: 1,
  WAIT_FOR_CONFIRMATION_REMOVAL: 2,
  WARN_NOT_CONFIRMED: 3,
  WARN_ALREADY_ASSIGNED_ACTION: 4,
  WARN_UNRECOGNIZED_KEY: 5,
};

/**
 * Maps a action to its pref name.
 * @const {!Object<SwitchAccessCommand, string>}
 * @private
 */
/* #export */ const actionToPref = {
  select: 'settings.a11y.switch_access.select.key_codes',
  next: 'settings.a11y.switch_access.next.key_codes',
  previous: 'settings.a11y.switch_access.previous.key_codes'
};

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
     * @private {Array<string>}
     */
    assignments_: {
      type: Array,
      value: [],
    },

    /**
     * The localized dialog title.
     * @private {string}
     */
    dialogTitle_: {
      type: String,
      computed: 'getLabelForAction_(action)',
    },

    /**
     * A dictionary containing all Switch Access key codes (mapped from
     * actions).
     * @private {{select: !Array<string>, next: !Array<string>, previous:
     *     !Array<string>}}
     */
    keyCodes_: {
      type: Object,
      value: {select: [], next: [], previous: []},
    },

    /**
     * User prompt text shown on the dialog.
     * @private {string}
     */
    promptText_: {
      type: String,
      computed: 'computePromptText_(assignmentState_)',
    },

    /** @private {?SwitchAccessCommand} */
    alreadyAssignedAction_: {
      type: String,
      value: null,
    },

    /** @private {?string} */
    currentKey_: {
      type: String,
      value: null,
    },

    /** @private {?string} */
    unexpectedKey_: {
      type: String,
      value: null,
    },

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
      case AssignmentState.WARN_ALREADY_ASSIGNED_ACTION:
      case AssignmentState.WARN_UNRECOGNIZED_KEY:
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
      if (!this.keyCodes_[action].includes(event.keyCode)) {
        continue;
      }

      if (action === this.action) {
        this.assignmentState_ = AssignmentState.WAIT_FOR_CONFIRMATION_REMOVAL;
      } else {
        this.alreadyAssignedAction_ = action;
        this.assignmentState_ = AssignmentState.WARN_ALREADY_ASSIGNED_ACTION;
      }
      return;
    }
    this.assignmentState_ = AssignmentState.WAIT_FOR_CONFIRMATION;
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
      this.keyCodes_[this.action].push(this.currentKeyCode_);
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
      this.assignmentState_ = AssignmentState.WARN_NOT_CONFIRMED;
      return;
    }

    // Remove this key code.
    const index = this.keyCodes_[this.action].indexOf(this.currentKeyCode_);
    if (index !== -1) {
      this.keyCodes_[this.action].splice(index, 1);
    }
    this.$.switchAccessActionAssignmentDialog.close();
  },

  /** @private */
  onCancelClick_() {
    this.$.switchAccessActionAssignmentDialog.close();
  },

  /**
   * @param {!Object<SwitchAccessCommand, !Array<string>>} value
   * @private
   */
  onAssignmentsChanged_(value) {
    switch (this.action) {
      case SwitchAccessCommand.SELECT:
        this.assignments_ = value.select;
        break;
      case SwitchAccessCommand.NEXT:
        this.assignments_ = value.next;
        break;
      case SwitchAccessCommand.PREVIOUS:
        this.assignments_ = value.previous;
        break;
    }

    if (!this.assignments_.length) {
      this.assignments_ = [this.i18n('noSwitchesAssigned')];
    }
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
    }
  },

  /**
   * @param {AssignmentState} assignmentState
   * @return {string}
   * @private
   */
  computePromptText_(assignmentState) {
    switch (assignmentState) {
      case AssignmentState.WAIT_FOR_KEY:
        return this.i18n('switchAccessActionAssignmentDialogWaitForKeyPrompt');
      case AssignmentState.WAIT_FOR_CONFIRMATION:
        return this.i18n(
            'switchAccessActionAssignmentDialogWaitForConfirmationPrompt',
            this.currentKey_);
      case AssignmentState.WAIT_FOR_CONFIRMATION_REMOVAL:
        return this.i18n(
            'switchAccessActionAssignmentDialogWaitForConfirmationRemovalPrompt',
            this.currentKey_);
      case AssignmentState.WARN_NOT_CONFIRMED:
        return this.i18n(
            'switchAccessActionAssignmentDialogWarnNotConfirmedPrompt',
            this.unexpectedKey_, this.currentKey_);
      case AssignmentState.WARN_ALREADY_ASSIGNED_ACTION:
        return this.i18n(
            'switchAccessActionAssignmentDialogWarnAlreadyAssignedActionPrompt',
            this.currentKey_,
            this.getLabelForAction_(this.alreadyAssignedAction_));
      case AssignmentState.WARN_UNRECOGNIZED_KEY:
        return this.i18n(
            'switchAccessActionAssignmentDialogWarnUnrecognizedKeyPrompt');
    }
  },
});
