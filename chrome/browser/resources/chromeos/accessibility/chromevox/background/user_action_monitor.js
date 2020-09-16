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
    if (this.actions_[0].opt_beforeActionCallback) {
      this.actions_[0].opt_beforeActionCallback();
    }
  }

  // Public methods.

  /**
   * Returns true if the key sequence was matched. Returns false otherwise.
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
    return true;
  }

  // Private methods.

  /** @private */
  expectedActionMatched_() {
    const action = this.getExpectedAction_();
    if (action.opt_afterActionCallback) {
      action.opt_afterActionCallback();
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
    if (action.opt_beforeActionCallback) {
      action.opt_beforeActionCallback();
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
 *    beforeActionMsg: (string|undefined),
 *    afterActionMsg: (string|undefined)
 * }}
 */
UserActionMonitor.ActionInfo;

// Represents an expected action.
UserActionMonitor.Action = class {
  /**
   * @param {ActionType} type
   * @param {string|!KeySequence} value
   * @param {(function():void)=} opt_beforeActionCallback Runs once before this
   *     action is seen.
   * @param {(function():void)=} opt_afterActionCallback Runs once after this
   *     action is seen.
   */
  constructor(type, value, opt_beforeActionCallback, opt_afterActionCallback) {
    switch (type) {
      case ActionType.KEY_SEQUENCE:
        if (!(value instanceof KeySequence)) {
          throw new Error(
              'UserActionMonitor: Must provide a KeySequence value for ' +
              'Actions of type ActionType.KEY_SEQUENCE');
        }
        break;
      default:
        if (typeof value !== 'string') {
          throw new Error(
              'UserActionMonitor: Must provide a string value for Actions ' +
              'if type is other than ActionType.KEY_SEQUENCE');
        }
    }

    /** @type {ActionType} */
    this.type = type;
    /** @type {string|!KeySequence} */
    this.value = value;
    /** @type {(function():void)|undefined} */
    this.opt_beforeActionCallback = opt_beforeActionCallback;
    /** @type {(function():void)|undefined} */
    this.opt_afterActionCallback = opt_afterActionCallback;
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
    const beforeActionMsg = info.beforeActionMsg;
    const afterActionMsg = info.afterActionMsg;

    const beforeActionCallback = () => {
      if (!beforeActionMsg) {
        return;
      }

      UserActionMonitor.Action.output_(beforeActionMsg);
    };

    const afterActionCallback = () => {
      if (!afterActionMsg) {
        return;
      }

      UserActionMonitor.Action.output_(afterActionMsg);
    };

    return new UserActionMonitor.Action(
        type, value, beforeActionCallback, afterActionCallback);
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
};