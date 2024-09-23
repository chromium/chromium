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
import {Command} from '../common/command.js';
import {CommandStore} from '../common/command_store.js';
import {GestureCommandData} from '../common/gesture_command_data.js';
import {KeyUtil} from '../common/key_util.js';
import {ChromeVoxKbHandler} from '../common/keyboard_handler.js';
import {Msgs} from '../common/msgs.js';
import {QueueMode, TtsSpeechProperties} from '../common/tts_types.js';

import Gesture = chrome.accessibilityPrivate.Gesture;
type Tab = chrome.tabs.Tab;

const TARGET = BridgeConstants.LearnMode.TARGET;
const Action = BridgeConstants.LearnMode.Action;

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
    // Export global objects from the background page context into this one.
    window.backgroundWindow = chrome.extension.getBackgroundPage() as Window;

    window.backgroundWindow.addEventListener(
        'keydown', LearnMode.onKeyDown, true);
    window.backgroundWindow.addEventListener('keyup', LearnMode.onKeyUp, true);
    window.backgroundWindow.addEventListener(
        'keypress', LearnMode.onKeyPress, true);
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

    BridgeHelper.registerHandler(
        TARGET, Action.CLEAR_TOUCH_EXPLORE_OUTPUT_TIME,
        () => MIN_TOUCH_EXPLORE_OUTPUT_TIME_MS = 0);
    BridgeHelper.registerHandler(
        TARGET, Action.ON_ACCESSIBILITY_GESTURE,
        (gesture: Gesture) => LearnMode.onAccessibilityGesture(gesture));
    BridgeHelper.registerHandler(
        TARGET, Action.ON_BRAILLE_KEY_EVENT,
        (event: chrome.brailleDisplayPrivate.KeyEvent) =>
          LearnMode.onBrailleKeyEvent(event));
    BridgeHelper.registerHandler(
        TARGET, Action.ON_KEY_DOWN,
        (event: KeyboardEvent) => LearnMode.onKeyDown(event));
    BridgeHelper.registerHandler(
        TARGET, Action.ON_KEY_UP,
        (event: KeyboardEvent) => LearnMode.onKeyUp(event));
    BridgeHelper.registerHandler(TARGET, Action.READY, () => readyPromise);

    readyCallback();
  }

  /**
   * Handles keydown events by speaking the human understandable name of the
   * key.
   * @return True if the default action should be performed.
   */
  static onKeyDown(evt: KeyboardEvent): boolean {
    // Process this event only once; it isn't a repeat (i.e. a user is holding a
    // key down).
    if (!evt.repeat) {
      LearnMode.output(KeyUtil.getReadableNameForKeyCode(evt.keyCode));

      // Allow Ctrl+W or escape to be handled.
      if ((evt.key === 'w' && evt.ctrlKey)) {
        LearnMode.close_();
        return true;
      }
      if (evt.key === 'Escape') {
        // Escape must be pressed twice in a row to exit.
        if (LearnMode.prevKey === 'Escape') {
          LearnMode.close_();
          return true;
        } else {
          // Append a message about pressing escape a second time.
          LearnMode.output(Msgs.getMsg('learn_mode_escape_to_exit'));
        }
      }
      LearnMode.prevKey = evt.key;

      BackgroundBridge.ForcedActionPath.onKeyDown(evt).then(
          (shouldPropagate) => {
            if (shouldPropagate) {
              ChromeVoxKbHandler.basicKeyDownActionsListener(evt);
            }
            LearnMode.clearRange();
          });
    }

    evt.preventDefault();
    evt.stopPropagation();
    return false;
  }

  static onKeyUp(evt: KeyboardEvent): void {
    LearnMode.shouldFlushSpeech_ = true;
    LearnMode.maybeClose_();
    LearnMode.clearRange();
    evt.preventDefault();
    evt.stopPropagation();
  }

  static onKeyPress(evt: KeyboardEvent): void {
    LearnMode.clearRange();
    evt.preventDefault();
    evt.stopPropagation();
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
  static output(text: string, outputCallback?: VoidFunction): void {
    BackgroundBridge.TtsBackground.speak(
        text,
        LearnMode.shouldFlushSpeech_ ? QueueMode.CATEGORY_FLUSH :
                                       QueueMode.QUEUE,
        new TtsSpeechProperties({endCallback: outputCallback}));
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
    window.backgroundWindow.removeEventListener(
        'keydown', LearnMode.onKeyDown, true);
    window.backgroundWindow.removeEventListener(
        'keyup', LearnMode.onKeyUp, true);
    window.backgroundWindow.removeEventListener(
        'keypress', LearnMode.onKeyPress, true);
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
    LearnMode.output(Msgs.getMsg('learn_mode_outtro'));
    LearnMode.resetListeners_();
    window.close();
  }
}

document.addEventListener('DOMContentLoaded', function(): void {
  LearnMode.init();
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


let readyCallback: VoidFunction;
const readyPromise =
    new Promise(resolve => readyCallback = resolve as VoidFunction);
