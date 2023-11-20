// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Forces user actions down a predetermined path.
 */
import {KeyCode} from '../../common/key_code.js';
import {BridgeConstants} from '../common/bridge_constants.js';
import {BridgeHelper} from '../common/bridge_helper.js';
import {Command} from '../common/command.js';
import {KeySequence, SerializedKeySequence} from '../common/key_sequence.js';
import {KeyUtil} from '../common/key_util.js';
import {PanelCommand, PanelCommandType} from '../common/panel_command.js';
import {QueueMode} from '../common/tts_types.js';

import {CommandHandlerInterface} from './input/command_handler_interface.js';
import {Output} from './output/output.js';

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
 * Monitors user actions and forces them down a predetermined path. Receives a
 * queue of expected actions upon construction and blocks ChromeVox execution
 * until each action is matched. Hooks into various handlers to intercept user
 * actions before they are processed by the rest of ChromeVox.
 */
export class ForcedActionPath {
  /**
   * @param {!Array<ActionInfo>} actionInfos A queue of expected actions.
   * @param {function():void} onFinishedCallback Runs once after all expected
   *     actions have been matched.
   */
  constructor(actionInfos, onFinishedCallback) {
    if (actionInfos.length === 0) {
      throw new Error(`ForcedActionPath: actionInfos can't be empty`);
    }

    /** @private {number} */
    this.actionIndex_ = 0;
    /** @private {!Array<!Action>} */
    this.actions_ = [];
    /** @private {function():void} */
    this.onFinishedCallback_ = onFinishedCallback;

    for (let i = 0; i < actionInfos.length; ++i) {
      this.actions_.push(ForcedActionPath.createAction(actionInfos[i]));
    }
    if (this.actions_[0].beforeActionCallback) {
      this.actions_[0].beforeActionCallback();
    }
  }

  // Static methods.

  /** @private */
  static closeChromeVox_() {
    (new PanelCommand(PanelCommandType.CLOSE_CHROMEVOX)).send();
  }

  /**
   * Creates a new forced action path.
   * @param {!Array<{
   *    type: string,
   *    value: (string|Object),
   *    beforeActionMsg: (string|undefined),
   *    afterActionMsg: (string|undefined)
   * }>} actions
   * @param {function(): void} callback
   */
  static create(actions, callback) {
    if (ForcedActionPath.instance) {
      throw 'Error: trying to create a second ForcedActionPath';
    }
    ForcedActionPath.instance = new ForcedActionPath(actions, callback);
  }

  /** Destroys the forced action path. */
  static destroy() {
    ForcedActionPath.instance = null;
  }

  /**
   * Constructs a new Action given an ActionInfo object.
   * @param {!ActionInfo} info
   * @return {!Action}
   */
  static createAction(info) {
    switch (info.type) {
      case ActionType.KEY_SEQUENCE:
        if (typeof info.value !== 'object') {
          throw new Error(
              'ForcedActionPath: Must provide an object resembling a ' +
              'KeySequence for Actions of type ActionType.KEY_SEQUENCE');
        }
        break;

      default:
        if (typeof info.value !== 'string') {
          throw new Error(
              'ForcedActionPath: Must provide a string value for Actions if ' +
              'type is other than ActionType.KEY_SEQUENCE');
        }
    }

    const type = info.type;
    const value = (typeof info.value === 'string') ?
        info.value :
        KeySequence.deserialize(
            /** @type {!SerializedKeySequence} */ (info.value));
    const shouldPropagate = info.shouldPropagate;
    const beforeActionMsg = info.beforeActionMsg;
    const afterActionMsg = info.afterActionMsg;
    const afterActionCmd = info.afterActionCmd;

    const beforeActionCallback = () => {
      if (!beforeActionMsg) {
        return;
      }

      Action.output_(beforeActionMsg);
    };

    // A function that either provides output or performs a command when the
    // action has been matched.
    const afterActionCallback = () => {
      if (afterActionMsg) {
        Action.output_(afterActionMsg);
      } else if (afterActionCmd) {
        Action.onCommand_(afterActionCmd);
      }
    };

    const params = {
      type,
      value,
      shouldPropagate,
      beforeActionCallback,
      afterActionCallback,
    };

    switch (type) {
      case ActionType.KEY_SEQUENCE:
        return new KeySequenceAction(params);
      default:
        return new StringAction(params);
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
    if (actualSequence.equals(CLOSE_CHROMEVOX_KEY_SEQUENCE)) {
      ForcedActionPath.closeChromeVox_();
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

  /**
   * @param {Event} evt The key down event to process.
   * @return {boolean} Whether the event should continue propagating.
   */
  onKeyDown(evt) {
    const keySequence = KeyUtil.keyEventToKeySequence(evt);
    return this.onKeySequence(keySequence);
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
          `ForcedActionPath: can't call nextAction_(), invalid index`);
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
   * @return {!Action}
   * @private
   */
  getExpectedAction_() {
    if (this.actionIndex_ >= 0 && this.actionIndex_ < this.actions_.length) {
      return this.actions_[this.actionIndex_];
    }

    throw new Error('ForcedActionPath: actionIndex_ is invalid.');
  }
}

/** @type {ForcedActionPath} */
ForcedActionPath.instance;

// Local to module.

/**
 * The key sequence used to close ChromeVox.
 * @const {!KeySequence}
 * @private
 */
const CLOSE_CHROMEVOX_KEY_SEQUENCE = KeySequence.deserialize(
    {keys: {keyCode: [KeyCode.Z], ctrlKey: [true], altKey: [true]}});

/**
 * Defines an object that is used to create a ForcedActionPath Action.
 * @typedef {{
 *    type: ActionType,
 *    value: (string|Object),
 *    shouldPropagate: (boolean|undefined),
 *    beforeActionMsg: (string|undefined),
 *    afterActionMsg: (string|undefined),
 *    afterActionCmd: (!Command|undefined),
 * }}
 */
let ActionInfo;

/**
 * Represents an expected action.
 * @abstract
 */
class Action {
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
   * @protected
   */
  constructor(params) {
    /** @type {ActionType} */
    this.type = params.type;
    /** @type {string|!KeySequence} */
    this.value = this.typedValue(params.value);
    /** @type {boolean} */
    this.shouldPropagate =
        (params.shouldPropagate !== undefined) ? params.shouldPropagate : true;
    /** @type {(function():void)|undefined} */
    this.beforeActionCallback = params.beforeActionCallback;
    /** @type {(function():void)|undefined} */
    this.afterActionCallback = params.afterActionCallback;
  }

  /**
   * @param {!Action} other
   * @return {boolean}
   */
  equals(other) {
    return this.type === other.type;
  }

  /**
   * @param {string|Object} value
   * @return {string|!KeySequence}
   * @abstract
   */
  typedValue(value) {}

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
   * @param {!Command} command
   * @private
   */
  static onCommand_(command) {
    CommandHandlerInterface.instance.onCommand(command);
  }
}

class KeySequenceAction extends Action {
  /** @override */
  equals(other) {
    return super.equals(other) &&
        this.value.equals(/**@type {!KeySequence} */ (other.value));
  }

  /** @override */
  typedValue(value) {
    if (!(value instanceof KeySequence)) {
      throw new Error(
          'ForcedActionPath: Must provide a KeySequence value for ' +
          'Actions of type ActionType.KEY_SEQUENCE');
    }
    return /** @type {!KeySequence} */ (value);
  }
}

class StringAction extends Action {
  /** @override */
  equals(other) {
    return super.equals(other) && this.value === other.value;
  }

  /** @override */
  typedValue(value) {
    if (typeof value !== 'string') {
      throw new Error(`ForcedActionPath: Must provide string value for ${
          this.type} actions`);
    }
    return String(value);
  }
}

BridgeHelper.registerHandler(
    BridgeConstants.ForcedActionPath.TARGET,
    BridgeConstants.ForcedActionPath.Action.CREATE,
    actions =>
        new Promise(resolve => ForcedActionPath.create(actions, resolve)));
BridgeHelper.registerHandler(
    BridgeConstants.ForcedActionPath.TARGET,
    BridgeConstants.ForcedActionPath.Action.DESTROY,
    () => ForcedActionPath.destroy());
BridgeHelper.registerHandler(
    BridgeConstants.ForcedActionPath.TARGET,
    BridgeConstants.ForcedActionPath.Action.ON_KEY_DOWN,
    evt => ForcedActionPath.instance?.onKeyDown(evt) ?? true);
