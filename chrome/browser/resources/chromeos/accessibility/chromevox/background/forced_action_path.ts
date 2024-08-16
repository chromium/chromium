// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Forces user actions down a predetermined path.
 */
import {BridgeHelper} from '/common/bridge_helper.js';
import {KeyCode} from '/common/key_code.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BridgeConstants} from '../common/bridge_constants.js';
import {Command} from '../common/command.js';
import {KeySequence, SerializedKeySequence} from '../common/key_sequence.js';
import {KeyUtil} from '../common/key_util.js';
import {PanelCommand, PanelCommandType} from '../common/panel_command.js';
import {QueueMode} from '../common/tts_types.js';

import {CommandHandlerInterface} from './input/command_handler_interface.js';
import {Output} from './output/output.js';

/** The types of actions we want to monitor. */
enum ActionType {
  BRAILLE = 'braille',
  GESTURE = 'gesture',
  KEY_SEQUENCE = 'key_sequence',
  MOUSE_EVENT = 'mouse_event',
  RANGE_CHANGE = 'range_change',
}

/**
 * Monitors user actions and forces them down a predetermined path. Receives a
 * queue of expected actions upon construction and blocks ChromeVox execution
 * until each action is matched. Hooks into various handlers to intercept user
 * actions before they are processed by the rest of ChromeVox.
 */
export class ForcedActionPath {
  private actionIndex_: number = 0;
  private actions_: Action[] = [];
  private onFinishedCallback_: VoidFunction;

  static instance?: ForcedActionPath|null;
  static postGestureCallbackForTesting?: VoidFunction;
  static postKeyDownEventCallbackForTesting?: VoidFunction;

  /**
   * @param actionInfos A queue of expected actions.
   * @param onFinishedCallback Runs once after all expected actions have been
   *     matched.
   */
  constructor(actionInfos: ActionInfo[], onFinishedCallback: VoidFunction) {
    if (actionInfos.length === 0) {
      throw new Error(`ForcedActionPath: actionInfos can't be empty`);
    }

    for (let i = 0; i < actionInfos.length; ++i) {
      this.actions_.push(ForcedActionPath.createAction(actionInfos[i]!));
    }
    if (this.actions_[0]!.beforeActionCallback) {
      this.actions_[0]!.beforeActionCallback();
    }

    this.onFinishedCallback_ = onFinishedCallback;
  }

  // Static methods.

  private static closeChromeVox_(): void {
    (new PanelCommand(PanelCommandType.CLOSE_CHROMEVOX)).send();
  }

  static listenFor(actions: ActionInfo[]): Promise<void> {
    if (ForcedActionPath.instance) {
      throw 'Error: trying to create a second ForcedActionPath';
    }
    return new Promise<void>(
        resolve => ForcedActionPath.instance =
            new ForcedActionPath(actions, resolve));
  }

  /** Destroys the forced action path. */
  static stopListening(): void {
    ForcedActionPath.instance = null;
  }

  /**
   * Constructs a new Action given an ActionInfo object.
   * @param {!ActionInfo} info
   * @return {!Action}
   */
  static createAction(info: ActionInfo): Action{
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
        KeySequence.deserialize(info.value as SerializedKeySequence);
    const shouldPropagate = info.shouldPropagate;
    const beforeActionMsg = info.beforeActionMsg;
    const afterActionMsg = info.afterActionMsg;
    const afterActionCmd = info.afterActionCmd;

    const beforeActionCallback = (): void => {
      if (!beforeActionMsg) {
        return;
      }

      output(beforeActionMsg);
    };

    // A function that either provides output or performs a command when the
    // action has been matched.
    const afterActionCallback = (): void => {
      if (afterActionMsg) {
        output(afterActionMsg);
      } else if (afterActionCmd) {
        onCommand(afterActionCmd);
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
   */
  onKeySequence(actualSequence: KeySequence): boolean {
    if (actualSequence.equals(CLOSE_CHROMEVOX_KEY_SEQUENCE)) {
      ForcedActionPath.closeChromeVox_();
      return true;
    }

    const expectedAction = this.getExpectedAction_();
    if (expectedAction.type !== ActionType.KEY_SEQUENCE) {
      return false;
    }

    const expectedSequence = expectedAction.value as KeySequence;
    if (!expectedSequence.equals(actualSequence)) {
      return false;
    }

    this.expectedActionMatched_();
    return Boolean(expectedAction.shouldPropagate);
  }

  /**
   * Returns true if the gesture should be allowed to propagate, false
   * otherwise.
   */
  onGesture(actualGesture: string): boolean {
    const expectedAction = this.getExpectedAction_();
    if (expectedAction.type !== ActionType.GESTURE) {
      if (ForcedActionPath.postGestureCallbackForTesting) {
        ForcedActionPath.postGestureCallbackForTesting();
      }
      return false;
    }

    const expectedGesture = expectedAction.value;
    if (expectedGesture !== actualGesture) {
      if (ForcedActionPath.postGestureCallbackForTesting) {
        ForcedActionPath.postGestureCallbackForTesting();
      }
      return false;
    }

    this.expectedActionMatched_();
    if (ForcedActionPath.postGestureCallbackForTesting) {
      ForcedActionPath.postGestureCallbackForTesting();
    }
    return expectedAction.shouldPropagate;
  }

  /** @return Whether the event should continue propagating. */
  onKeyDown(evt: KeyboardEvent): boolean {
    const keySequence = KeyUtil.keyEventToKeySequence(evt);
    const result = this.onKeySequence(keySequence);
    if (ForcedActionPath.postKeyDownEventCallbackForTesting) {
      ForcedActionPath.postKeyDownEventCallbackForTesting();
    }
    return result;
  }

  // Private methods.

  private expectedActionMatched_(): void {
    const action = this.getExpectedAction_();
    if (action.afterActionCallback) {
      action.afterActionCallback();
    }

    this.nextAction_();
  }

  private nextAction_(): void {
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

  private onAllMatched_(): void {
    this.onFinishedCallback_();
  }

  private getExpectedAction_(): Action {
    if (this.actionIndex_ >= 0 && this.actionIndex_ < this.actions_.length) {
      return this.actions_[this.actionIndex_];
    }

    throw new Error('ForcedActionPath: actionIndex_ is invalid.');
  }
}

// Local to module.

/** The key sequence used to close ChromeVox. */
const CLOSE_CHROMEVOX_KEY_SEQUENCE = KeySequence.deserialize(
    {keys: {keyCode: [KeyCode.Z], ctrlKey: [true], altKey: [true]}});

type SerializedValueType = string | Object;
type ValueType = string | KeySequence;

/** Defines an object that is used to create a ForcedActionPath Action. */
interface ActionInfo {
  type: ActionType;
  value: SerializedValueType;
  shouldPropagate?: boolean;
  beforeActionMsg?: string;
  afterActionMsg?: string;
  afterActionCmd?: Command;
}

interface ActionParamsInternal {
  type: ActionType;
  value: ValueType;
  shouldPropagate?: boolean;
  beforeActionCallback?: VoidFunction;
  afterActionCallback?: VoidFunction;
}

abstract class Action {
  type: ActionType;
  value: ValueType;
  shouldPropagate: boolean;
  beforeActionCallback?: VoidFunction;
  afterActionCallback?: VoidFunction;

  /**
   * Please see below for more information on arguments:
   * type: The type of action.
   * value: The action value.
   * shouldPropagate: Whether or not this action should propagate to other
   *  handlers e.g. CommandHandler.
   * beforeActionCallback: A callback that runs once before this action is seen.
   * afterActionCallback: A callback that runs once after this action is seen.
   */
  constructor(params: ActionParamsInternal) {
    this.type = params.type;
    this.value = this.typedValue(params.value);
    this.shouldPropagate =
        (params.shouldPropagate !== undefined) ? params.shouldPropagate : true;
    this.beforeActionCallback = params.beforeActionCallback;
    this.afterActionCallback = params.afterActionCallback;
  }

  equals(other: Action): boolean {
    return this.type === other.type;
  }

  abstract typedValue(value: SerializedValueType): ValueType;
}

class KeySequenceAction extends Action {
  override equals(other: Action): boolean {
    return super.equals(other) &&
        (this.value as KeySequence).equals(other.value as KeySequence);
  }

  override typedValue(value: SerializedValueType): KeySequence {
    if (!(value instanceof KeySequence)) {
      throw new Error(
          'ForcedActionPath: Must provide a KeySequence value for ' +
          'Actions of type ActionType.KEY_SEQUENCE');
    }
    return value;
  }
}

class StringAction extends Action {
  override equals(other: Action): boolean {
    return super.equals(other) && this.value === other.value;
  }

  override typedValue(value: SerializedValueType): string {
    if (typeof value !== 'string') {
      throw new Error(`ForcedActionPath: Must provide string value for ${
          this.type} actions`);
    }
    return value;
  }
}

// Local to module.

/** Uses Output module to provide speech and braille feedback. */
function output(message: string): void {
  new Output().withString(message).withQueueMode(QueueMode.FLUSH).go();
}

/** Uses the CommandHandler to perform a command. */
function onCommand(command: Command): void {
  CommandHandlerInterface.instance.onCommand(command);
}

BridgeHelper.registerHandler(
    BridgeConstants.ForcedActionPath.TARGET,
    BridgeConstants.ForcedActionPath.Action.LISTEN_FOR,
    (actions: ActionInfo[]) => ForcedActionPath.listenFor(actions));
BridgeHelper.registerHandler(
    BridgeConstants.ForcedActionPath.TARGET,
    BridgeConstants.ForcedActionPath.Action.STOP_LISTENING,
    () => ForcedActionPath.stopListening());
BridgeHelper.registerHandler(
    BridgeConstants.ForcedActionPath.TARGET,
    BridgeConstants.ForcedActionPath.Action.ON_KEY_DOWN,
    (evt: KeyboardEvent) => ForcedActionPath.instance?.onKeyDown(evt) ?? true);

TestImportManager.exportForTesting(ForcedActionPath);
