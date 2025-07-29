// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script for ChromeOS keyboard explorer.
 *
 */
import {BridgeHelper} from '/common/bridge_helper.js';

import {BackgroundBridge} from '../common/background_bridge.js';
import {BrailleCommandData} from '../common/braille/braille_command_data.js';
import {BrailleKeyEvent} from '../common/braille/braille_key_types.js';
import {BridgeConstants} from '../common/bridge_constants.js';
import type {Command} from '../common/command.js';
import {CommandStore} from '../common/command_store.js';
import {GestureCommandData} from '../common/gesture_command_data.js';
import {InternalKeyEvent} from '../common/internal_key_event.js'
import {KeyUtil} from '../common/key_util.js';
import {ChromeVoxKbHandler} from '../common/keyboard_handler.js';
import {Msgs} from '../common/msgs.js';
import {OffscreenBridge} from '../common/offscreen_bridge.js';
import {QueueMode, TtsSpeechProperties} from '../common/tts_types.js';

import Gesture = chrome.accessibilityPrivate.Gesture;
type Tab = chrome.tabs.Tab;

const TARGET = BridgeConstants.LearnMode.TARGET;
const Action = BridgeConstants.LearnMode.Action;
const TestTARGET = BridgeConstants.LearnModeTest.TARGET;
const TestAction = BridgeConstants.LearnModeTest.Action;

declare namespace window {
  let backgroundWindow: Window;
  function close(): void;
}

/** Class to manage the keyboard explorer. */
export class LearnMode {
  /** Indicates when speech output should flush previous speech. */
  private static shouldFlushSpeech_ = true;
  /** Last time a touch explore gesture was described. */
  private static lastTouchExplore_ = new Date();
  /** Previously pressed key. */
  private static prevKey = '';

  /** Initialize keyboard explorer. */
  static init(): void {
    // Listen to all key events on the offscreen document.
    OffscreenBridge.learnModeRegisterListeners();

    chrome.brailleDisplayPrivate.onKeyEvent.addListener(
        LearnMode.onBrailleKeyEvent);
    chrome.accessibilityPrivate.onAccessibilityGesture.addListener(
        LearnMode.onAccessibilityGesture);
    chrome.accessibilityPrivate.setKeyboardListener(true, true);
    BackgroundBridge.Braille.setBypass(true);
    BackgroundBridge.GestureCommandHandler.setBypass(true);

    ChromeVoxKbHandler.commandHandler = LearnMode.onCommand;

    // TODO(b/314203187): Not null asserted, check that this is correct.
    $('instruction')!.textContent = Msgs.getMsg('learn_mode_intro');

    // Learn mode may be created more than once. Clear the listeners to avoid
    // duplicate assignment errors.
    BridgeHelper.clearAllHandlersForTarget(TARGET);
    BridgeHelper.clearAllHandlersForTarget(TestTARGET);

    BridgeHelper.registerHandler(
        TARGET, Action.ON_KEY_DOWN,
        (internalEvent: InternalKeyEvent) =>
            LearnMode.onKeyDown(internalEvent));
    BridgeHelper.registerHandler(
        TARGET, Action.ON_KEY_UP, () => LearnMode.onKeyUp());
    BridgeHelper.registerHandler(
        TARGET, Action.ON_KEY_PRESS, () => LearnMode.onKeyPress());

    // The following BridgeHelper handlers are only used for testing.
    BridgeHelper.registerHandler(
        TestTARGET, TestAction.CLEAR_TOUCH_EXPLORE_OUTPUT_TIME,
        () => MIN_TOUCH_EXPLORE_OUTPUT_TIME_MS = 0);
    BridgeHelper.registerHandler(
        TestTARGET, TestAction.ON_ACCESSIBILITY_GESTURE,
        (gesture: Gesture) => LearnMode.onAccessibilityGesture(gesture));
    BridgeHelper.registerHandler(
        TestTARGET, TestAction.ON_BRAILLE_KEY_EVENT,
        (event: chrome.brailleDisplayPrivate.KeyEvent) =>
            LearnMode.onBrailleKeyEvent(event));
  }

  /**
   * Handles keydown events by speaking the human understandable name of the
   * key.
   * @param evt Serialized key event sent from offscreen document.
   * @return boolean True to stop event propogation, false otherwise.
   */
  static onKeyDown(evt: InternalKeyEvent): boolean {
    // Process this event only once; it isn't a repeat (i.e. a user is holding a
    // key down).
    if (!evt.repeat) {
      LearnMode.output(KeyUtil.getReadableNameForKeyCode(evt.keyCode));

      // Allow Ctrl+W or escape to be handled.
      if ((evt.key === 'w' && evt.ctrlKey)) {
        LearnMode.close_();
        return false;
      }
      if (evt.key === 'Escape') {
        // Escape must be pressed twice in a row to exit.
        if (LearnMode.prevKey === 'Escape') {
          LearnMode.close_();
          return false;
        } else {
          // Append a message about pressing escape a second time.
          LearnMode.output(Msgs.getMsg('learn_mode_escape_to_exit'));
        }
      }
      LearnMode.prevKey = evt.key;

      BackgroundBridge.ForcedActionPath.onKeyDown(evt).then(
          (shouldPropagate) => {
            if (shouldPropagate) {
              ChromeVoxKbHandler.basicKeyDownActionsListener(
                  new InternalKeyEvent(evt));
            }
            LearnMode.clearRange();
          });
    }

    return true;
  }

  static onKeyUp(): void {
    LearnMode.shouldFlushSpeech_ = true;
    LearnMode.maybeClose_();
    LearnMode.clearRange();
  }

  static onKeyPress(): void {
    LearnMode.clearRange();
  }

  static onBrailleKeyEvent(evt: chrome.brailleDisplayPrivate.KeyEvent): void {
    LearnMode.shouldFlushSpeech_ = true;
    LearnMode.maybeClose_();
    let msgid;
    const msgArgs: string[] = [];
    let text;
    let callback;
    switch (evt.command) {
      case chrome.brailleDisplayPrivate.KeyCommand.PAN_LEFT:
        msgid = 'braille_pan_left';
        break;
      case chrome.brailleDisplayPrivate.KeyCommand.PAN_RIGHT:
        msgid = 'braille_pan_right';
        break;
      case chrome.brailleDisplayPrivate.KeyCommand.LINE_UP:
        msgid = 'braille_line_up';
        break;
      case chrome.brailleDisplayPrivate.KeyCommand.LINE_DOWN:
        msgid = 'braille_line_down';
        break;
      case chrome.brailleDisplayPrivate.KeyCommand.TOP:
        msgid = 'braille_top';
        break;
      case chrome.brailleDisplayPrivate.KeyCommand.BOTTOM:
        msgid = 'braille_bottom';
        break;
      case chrome.brailleDisplayPrivate.KeyCommand.ROUTING:
      case chrome.brailleDisplayPrivate.KeyCommand.SECONDARY_ROUTING:
        msgid = 'braille_routing';
        // TODO(b/314203187): Not null asserted, check that this is correct.
        msgArgs.push(String(evt.displayPosition! + 1));
        break;
      case chrome.brailleDisplayPrivate.KeyCommand.CHORD:
        const dots = evt.brailleDots;
        if (!dots) {
          return;
        }

        // First, check for the dots mapping to a key code.
        const keyCode = BrailleKeyEvent.brailleChordsToStandardKeyCode[dots];
        if (keyCode) {
          if (keyCode === 'Escape') {
            callback = LearnMode.close_;
          }

          text = keyCode;
          break;
        }

        // Next, check for the modifier mappings.
        const mods = BrailleKeyEvent.brailleDotsToModifiers[dots];
        if (mods) {
          const outputs: string[] = [];
          for (const mod in mods) {
            if (mod === 'ctrlKey') {
              outputs.push('control');
            } else if (mod === 'altKey') {
              outputs.push('alt');
            } else if (mod === 'shiftKey') {
              outputs.push('shift');
            }
          }

          text = outputs.join(' ');
          break;
        }

        const command = BrailleCommandData.getCommand(dots);
        if (command && LearnMode.onCommand(command)) {
          return;
        }
        text = BrailleCommandData.makeShortcutText(dots, true);
        break;
      case chrome.brailleDisplayPrivate.KeyCommand.DOTS: {
        const dots = evt.brailleDots;
        if (!dots) {
          return;
        }
        const cells = new ArrayBuffer(1);
        const view = new Uint8Array(cells);
        view[0] = dots;
        BackgroundBridge.Braille.backTranslate(cells).then(res => {
          if (res !== null) {
            LearnMode.output(res);
          }
        });
      }
        return;
      case chrome.brailleDisplayPrivate.KeyCommand.STANDARD_KEY:
        break;
    }
    if (msgid) {
      text = Msgs.getMsg(msgid, msgArgs);
    }

    LearnMode.output(text || evt.command, callback);
    LearnMode.clearRange();
  }

  /**
   * Handles accessibility gestures from the touch screen.
   * @param gesture The gesture to handle, based on the
   *     ax::mojom::Gesture enum defined in ui/accessibility/ax_enums.mojom
   */
  static onAccessibilityGesture(gesture: string): void {
    LearnMode.shouldFlushSpeech_ = true;
    LearnMode.maybeClose_();

    let callback;
    if (gesture === Gesture.TOUCH_EXPLORE) {
      if ((Number(new Date()) - Number(LearnMode.lastTouchExplore_)) <
          MIN_TOUCH_EXPLORE_OUTPUT_TIME_MS) {
        return;
      }
      LearnMode.lastTouchExplore_ = new Date();
    } else if (gesture === Gesture.SWIPE_LEFT2) {
      callback = LearnMode.close_;
    }

    const gestureData = GestureCommandData.GESTURE_COMMAND_MAP[gesture];
    if (gestureData) {
      if (gestureData.msgId) {
        LearnMode.output(Msgs.getMsg(gestureData.msgId));
      }
      if (gestureData.command) {
        LearnMode.onCommand(gestureData.command);
      }
      if (gestureData.commandDescriptionMsgId) {
        LearnMode.output(
            Msgs.getMsg(gestureData.commandDescriptionMsgId), callback);
      }
    }
  }

  /**
   * Queues up command description.
   * @return True if command existed and was handled.
   */
  static onCommand(command: Command): boolean | undefined {
    const msg = CommandStore.messageForCommand(command);
    if (msg) {
      const commandText = Msgs.getMsg(msg);
      LearnMode.output(commandText);
      LearnMode.clearRange();
      return true;
    }
    return undefined;
  }

  /** @param outputCallback A callback to run after output is requested. */
  static output(
      text: string, outputCallback?: VoidFunction,
      doNotInterrupt?: Boolean): void {
    BackgroundBridge.TtsBackground.speak(
        text,
        LearnMode.shouldFlushSpeech_ ? QueueMode.CATEGORY_FLUSH :
                                       QueueMode.QUEUE,
        new TtsSpeechProperties({doNotInterrupt, endCallback: outputCallback}));
    BackgroundBridge.Braille.write(text);
    LearnMode.shouldFlushSpeech_ = false;
    if (outputCallback) {
      outputCallback();
    }
  }

  /** Clears ChromeVox range. */
  static async clearRange(): Promise<void> {
    await BackgroundBridge.ChromeVoxRange.clearCurrentRange();
  }

  private static resetListeners_(): void {
    // Stop listening to key events on the offscreen document.
    OffscreenBridge.learnModeRemoveListeners();

    chrome.brailleDisplayPrivate.onKeyEvent.removeListener(
        LearnMode.onBrailleKeyEvent);
    chrome.accessibilityPrivate.onAccessibilityGesture.removeListener(
        LearnMode.onAccessibilityGesture);
    chrome.accessibilityPrivate.setKeyboardListener(true, false);
    BackgroundBridge.Braille.setBypass(false);
    BackgroundBridge.GestureCommandHandler.setBypass(false);
  }

  private static maybeClose_(): void {
    // Reset listeners and close this page if we somehow move outside of the
    // explorer window.
    chrome.windows.getLastFocused(
        {populate: true}, (focusedWindow: chrome.windows.Window) => {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      if (focusedWindow && focusedWindow.focused &&
          focusedWindow.tabs!.find((tab: Tab) => tab.url === location.href)) {
        return;
      }

      LearnMode.close_();
    });
  }

  private static close_(): void {
    LearnMode.output(
        Msgs.getMsg('learn_mode_outtro'), /*outputCallback=*/ undefined,
        /*doNotInterrupt=*/ true);
    LearnMode.resetListeners_();
    window.close();
  }
}

document.addEventListener('DOMContentLoaded', function(): void {
  LearnMode.init();
}, false);

// Key up and down events are handled offscreen and forwarded to LearnMode when
// it's active (see onkeyDown and onKeyUp above). However, we also receive DOM
// events directly in LearnMode, so we need to cancel these to prevent them from
// propagating.
document.addEventListener('keydown', (evt) => {
  evt.preventDefault();
  evt.stopPropagation();
}, false);

document.addEventListener('keyup', (evt) => {
  evt.preventDefault();
  evt.stopPropagation();
}, false);

// Local to module.

/**
 * Shortcut for document.getElementById.
 * @param {string} id of the element.
 * @return {Element} with the id.
 */
function $(id: string): HTMLElement | null {
  return document.getElementById(id);
}


/**
 * The minimum time to wait before describing another touch explore gesture.
 */
let MIN_TOUCH_EXPLORE_OUTPUT_TIME_MS = 1000;
