// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Monitors user actions.
 */

goog.provide('UserActionMonitor');

goog.require('KeyCode');
goog.require('KeySequence');
goog.require('Output');
goog.require('PanelCommand');
goog.require('PanelCommandType');

/**
 * The types of actions we want to monitor.
 * @enum {string}
 */
const ActionType = {
  BRAILLE: 'braille',
  GESTURE: 'gesture',
  KEY_SEQUENCE: 'key_sequence',
  MOUSE_EVENT: 'mouse_event',
  RANGE_CHANGE: 'range_change',
};

/**
 * Monitors user actions. Receives a queue of expected actions upon construction
 * and blocks ChromeVox execution until each action is matched. Hooks into
 * various handlers to intercept user actions before they are processed by the
 * rest of ChromeVox.
 */
UserActionMonitor = class {
  /**
   * @param {!Array<UserActionMonitor.ActionInfo>} actionInfos A queue of
   *     expected actions.
   * @param {function():void} onFinishedCallback Runs once after all expected
   *     actions have been matched.
   */
  constructor(actionInfos, onFinishedCallback) {
    if (actionInfos.length === 0) {
      throw new Error(`UserActionMonitor: actionInfos can't be empty`);
    }

    /** @private {number} */
    this.actionIndex_ = 0;
    /** @private {!Array<!UserActionMonitor.Action>} */
    this.actions_ = [];
    /** @private {function():void} */
    this.onFinishedCallback_ = onFinishedCallback;

    for (let i = 0; i < actionInfos.length; ++i) {
      this.actions_.push(
          UserActionMonitor.Action.fromActionInfo(actionInfos[i]));
    }
    if (this.actions_[0].beforeActionCallback) {
      this.actions_[0].beforeActionCallback();
    }
  }

  // Public methods.

  /**
   * Returns true if the key sequence should be allowed to propagate to other
   * handlers. Returns false otherwise.
   * @param {!KeySequence} actualSequence
   * @return {boolean}
   */
  onKeySequence(actualSequence) {
    if (actualSequence.equals(
            UserActionMonitor.CLOSE_CHROMEVOX_KEY_SEQUENCE_)) {
      UserActionMonitor.closeChromeVox_();
      return true;
    }

    const expectedAction = this.getExpectedAction_();
    if (expectedAction.type !== ActionType.KEY_SEQUENCE) {
      return false;
    }

    const expectedSequence = expectedAction.value;
    if (!expectedSequence.equals(actualSequence)) {
      return false;
    }

    this.expectedActionMatched_();
    return expectedAction.shouldPropagate;
  }

  /**
   * Returns true if the gesture should be allowed to propagate, false
   * otherwise.
   * @param {string} actualGesture
   * @return {boolean}
   */
  onGesture(actualGesture) {
    const expectedAction = this.getExpectedAction_();
    if (expectedAction.type !== ActionType.GESTURE) {
      return false;
    }

    const expectedGesture = expectedAction.value;
    if (expectedGesture !== actualGesture) {
      return false;
    }

    this.expectedActionMatched_();
    return expectedAction.shouldPropagate;
  }

  // Private methods.

  /** @private */
  expectedActionMatched_() {
    const action = this.getExpectedAction_();
    if (action.afterActionCallback) {
      action.afterActionCallback();
    }

    this.nextAction_();
  }

  /** @private */
  nextAction_() {
    if (this.actionIndex_ < 0 || this.actionIndex_ >= this.actions_.length) {
      throw new Error(
          `UserActionMonitor: can't call nextAction_(), invalid index`);
    }

    this.actionIndex_ += 1;
    if (this.actionIndex_ === this.actions_.length) {
      this.onAllMatched_();
      return;
    }

    const action = this.getExpectedAction_();
    if (action.beforeActionCallback) {
      action.beforeActionCallback();
    }
  }

  /** @private */
  onAllMatched_() {
    this.onFinishedCallback_();
  }

  /**
   * @return {!UserActionMonitor.Action}
   * @private
   */
  getExpectedAction_() {
    if (this.actionIndex_ >= 0 && this.actionIndex_ < this.actions_.length) {
      return this.actions_[this.actionIndex_];
    }

    throw new Error('UserActionMonitor: actionIndex_ is invalid.');
  }

  /** @private */
  static closeChromeVox_() {
    (new PanelCommand(PanelCommandType.CLOSE_CHROMEVOX)).send();
  }
};

/**
 * The key sequence used to close ChromeVox.
 * @const {!KeySequence}
 * @private
 */
UserActionMonitor.CLOSE_CHROMEVOX_KEY_SEQUENCE_ = KeySequence.deserialize(
    {keys: {keyCode: [KeyCode.Z], ctrlKey: [true], altKey: [true]}});

/**
 * Defines an object that is used to create a UserActionMonitor.Action.
 * @typedef {{
 *    type: ActionType,
 *    value: (string|Object),
 *    shouldPropagate: (boolean|undefined),
 *    beforeActionMsg: (string|undefined),
 *    afterActionMsg: (string|undefined),
 *    afterActionCmd: (string|undefined),
 * }}
 */
UserActionMonitor.ActionInfo;

// Represents an expected action.
UserActionMonitor.Action = class {
  /**
   * Please see below for more information on arguments:
   * type: The type of action.
   * value: The action value.
   * shouldPropagate: Whether or not this action should propagate to other
   *  handlers e.g. CommandHandler.
   * beforeActionCallback: A callback that runs once before this action is seen.
   * afterActionCallback: A callback that runs once after this action is seen.
   * @param {!{
   *  type: ActionType,
   *  value: (string|!KeySequence),
   *  shouldPropagate: (boolean|undefined),
   *  beforeActionCallback: (function(): void|undefined),
   *  afterActionCallback: (function(): void|undefined)
   * }} params
   */
  constructor(params) {
    /** @type {ActionType} */
    this.type = params.type;
    /** @type {string|!KeySequence} */
    this.value = params.value;
    /** @type {boolean} */
    this.shouldPropagate =
        (params.shouldPropagate !== undefined) ? params.shouldPropagate : true;
    /** @type {(function():void)|undefined} */
    this.beforeActionCallback = params.beforeActionCallback;
    /** @type {(function():void)|undefined} */
    this.afterActionCallback = params.afterActionCallback;

    switch (this.type) {
      case ActionType.KEY_SEQUENCE:
        if (!(this.value instanceof KeySequence)) {
          throw new Error(
              'UserActionMonitor: Must provide a KeySequence value for ' +
              'Actions of type ActionType.KEY_SEQUENCE');
        }
        break;
      default:
        if (typeof this.value !== 'string') {
          throw new Error(
              'UserActionMonitor: Must provide a string value for Actions ' +
              'if type is other than ActionType.KEY_SEQUENCE');
        }
    }
  }

  /**
   * Constructs a new Action given an ActionInfo object.
   * @param {!UserActionMonitor.ActionInfo} info
   * @return {!UserActionMonitor.Action}
   */
  static fromActionInfo(info) {
    switch (info.type) {
      case ActionType.KEY_SEQUENCE:
        if (typeof info.value !== 'object') {
          throw new Error(
              'UserActionMonitor: Must provide an object resembling a ' +
              'KeySequence for Actions of type ActionType.KEY_SEQUENCE');
        }
        break;

      default:
        if (typeof info.value !== 'string') {
          throw new Error(
              'UserActionMonitor: Must provide a string value for Actions if ' +
              'type is other than ActionType.KEY_SEQUENCE');
        }
    }

    const type = info.type;
    const value = (typeof info.value === 'object') ?
        KeySequence.deserialize(info.value) :
        info.value;
    const shouldPropagate = info.shouldPropagate;
    const beforeActionMsg = info.beforeActionMsg;
    const afterActionMsg = info.afterActionMsg;
    const afterActionCmd = info.afterActionCmd;

    const beforeActionCallback = () => {
      if (!beforeActionMsg) {
        return;
      }

      UserActionMonitor.Action.output_(beforeActionMsg);
    };

    // A function that either provides output or performs a command when the
    // action has been matched.
    const afterActionCallback = () => {
      if (afterActionMsg) {
        UserActionMonitor.Action.output_(afterActionMsg);
      } else if (afterActionCmd) {
        UserActionMonitor.Action.onCommand_(afterActionCmd);
      }
    };

    return new UserActionMonitor.Action({
      type,
      value,
      shouldPropagate,
      beforeActionCallback,
      afterActionCallback
    });
  }

  /**
   * @param {!UserActionMonitor.Action} other
   * @return {boolean}
   */
  equals(other) {
    if (this.type !== other.type) {
      return false;
    }

    if (this.type === ActionType.KEY_SEQUENCE) {
      // For KeySequences, use the built-in equals method.
      return this.value.equals(/** @type {!KeySequence} */ (other.value));
    }

    return this.value === other.value;
  }

  // Static methods.

  /**
   * Uses Output module to provide speech and braille feedback.
   * @param {string} message
   * @private
   */
  static output_(message) {
    new Output().withString(message).withQueueMode(QueueMode.FLUSH).go();
  }

  /**
   * Uses the CommandHandler to perform a command.
   * @param {string} command
   * @private
   */
  static onCommand_(command) {
    CommandHandler.onCommand(command);
  }
};