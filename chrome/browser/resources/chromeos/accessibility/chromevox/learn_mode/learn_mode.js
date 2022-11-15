// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script for ChromeOS keyboard explorer.
 *
 */

import {BackgroundBridge} from '../common/background_bridge.js';
import {BrailleCommandData} from '../common/braille/braille_command_data.js';
import {BrailleKeyCommand, BrailleKeyEvent} from '../common/braille/braille_key_types.js';
import {NavBraille} from '../common/braille/nav_braille.js';
import {Command, CommandStore} from '../common/command_store.js';
import {GestureCommandData} from '../common/gesture_command_data.js';
import {KeyMap} from '../common/key_map.js';
import {KeyUtil} from '../common/key_util.js';
import {ChromeVoxKbHandler} from '../common/keyboard_handler.js';
import {Msgs} from '../common/msgs.js';
import {Spannable} from '../common/spannable.js';
import {QueueMode, TtsSpeechProperties} from '../common/tts_types.js';

/**
 * Class to manage the keyboard explorer.
 */
export class LearnMode {
  /**
   * Initialize keyboard explorer.
   */
  static async init() {
    // Export global objects from the background page context into this one.
    window.backgroundWindow = chrome.extension.getBackgroundPage();
    window.ChromeVox = window.backgroundWindow['ChromeVox'];

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
    BackgroundBridge.BrailleCommandHandler.setEnabled(false);
    BackgroundBridge.GestureCommandHandler.setEnabled(false);

    ChromeVoxKbHandler.commandHandler = LearnMode.onCommand;

    $('instruction').textContent = Msgs.getMsg('learn_mode_intro');
    LearnMode.shouldFlushSpeech_ = true;

    chrome.runtime.onMessage.addListener(message => {
      if (message['target'] !== 'LearnMode') {
        return;
      }

      switch (message['action']) {
        case 'onKeyDown':
        case 'onKeyUp':
        case 'onAccessibilityGesture':
        case 'onBrailleKeyEvent':
          LearnMode[message['action']].apply(LearnMode, message['args']);
          break;
        case 'clearTouchExploreOutputTime':
          LearnMode.MIN_TOUCH_EXPLORE_OUTPUT_TIME_MS_ = 0;
      }
    });
  }

  /**
   * Handles keydown events by speaking the human understandable name of the
   * key.
   * @param {Event} evt key event.
   * @return {boolean} True if the default action should be performed.
   */
  static onKeyDown(evt) {
    // Process this event only once; it isn't a repeat (i.e. a user is holding a
    // key down).
    if (!evt.repeat) {
      LearnMode.output(KeyUtil.getReadableNameForKeyCode(evt.keyCode));

      // Allow Ctrl+W or escape to be handled.
      if ((evt.key === 'w' && evt.ctrlKey) || evt.key === 'Escape') {
        LearnMode.close_();
        return true;
      }

      BackgroundBridge.UserActionMonitor.onKeyDown(evt).then(
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

  /**
   * Handles keyup events.
   * @param {Event} evt key event.
   */
  static onKeyUp(evt) {
    LearnMode.shouldFlushSpeech_ = true;
    LearnMode.maybeClose_();
    LearnMode.clearRange();
    evt.preventDefault();
    evt.stopPropagation();
  }

  /**
   * Handles keypress events.
   * @param {Event} evt key event.
   */
  static onKeyPress(evt) {
    LearnMode.clearRange();
    evt.preventDefault();
    evt.stopPropagation();
  }

  /**
   * @param {BrailleKeyEvent} evt The key event.
   */
  static onBrailleKeyEvent(evt) {
    LearnMode.shouldFlushSpeech_ = true;
    LearnMode.maybeClose_();
    let msgid;
    const msgArgs = [];
    let text;
    let callback;
    switch (evt.command) {
      case BrailleKeyCommand.PAN_LEFT:
        msgid = 'braille_pan_left';
        break;
      case BrailleKeyCommand.PAN_RIGHT:
        msgid = 'braille_pan_right';
        break;
      case BrailleKeyCommand.LINE_UP:
        msgid = 'braille_line_up';
        break;
      case BrailleKeyCommand.LINE_DOWN:
        msgid = 'braille_line_down';
        break;
      case BrailleKeyCommand.TOP:
        msgid = 'braille_top';
        break;
      case BrailleKeyCommand.BOTTOM:
        msgid = 'braille_bottom';
        break;
      case BrailleKeyCommand.ROUTING:
      case BrailleKeyCommand.SECONDARY_ROUTING:
        msgid = 'braille_routing';
        msgArgs.push(/** @type {number} */ (evt.displayPosition + 1));
        break;
      case BrailleKeyCommand.CHORD:
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
          const outputs = [];
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
      case BrailleKeyCommand.DOTS: {
        const dots = evt.brailleDots;
        if (!dots) {
          return;
        }
        const cells = new ArrayBuffer(1);
        const view = new Uint8Array(cells);
        view[0] = dots;
        BackgroundBridge.BrailleBackground.backTranslate(cells).then(res => {
          if (res !== null) {
            LearnMode.output(res);
          }
        });
      }
        return;
      case BrailleKeyCommand.STANDARD_KEY:
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
   * @param {string} gesture The gesture to handle, based on the
   *     ax::mojom::Gesture enum defined in ui/accessibility/ax_enums.mojom
   */
  static onAccessibilityGesture(gesture) {
    LearnMode.shouldFlushSpeech_ = true;
    LearnMode.maybeClose_();

    let callback;
    if (gesture === chrome.accessibilityPrivate.Gesture.TOUCH_EXPLORE) {
      if ((new Date() - LearnMode.lastTouchExplore_) <
          LearnMode.MIN_TOUCH_EXPLORE_OUTPUT_TIME_MS_) {
        return;
      }
      LearnMode.lastTouchExplore_ = new Date();
    } else if (gesture === chrome.accessibilityPrivate.Gesture.SWIPE_LEFT2) {
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
   * @param {!Command} command
   * @return {boolean|undefined} True if command existed and was handled.
   */
  static onCommand(command) {
    const msg = CommandStore.messageForCommand(command);
    if (msg) {
      const commandText = Msgs.getMsg(msg);
      LearnMode.output(commandText);
      LearnMode.clearRange();
      return true;
    }
  }

  /**
   * @param {string} text
   * @param {function()=} opt_speakCallback A callback to run when speech
   *     finishes.
   */
  static output(text, opt_speakCallback) {
    const ChromeVox = window.ChromeVox;
    ChromeVox.tts.speak(
        text,
        LearnMode.shouldFlushSpeech_ ? QueueMode.CATEGORY_FLUSH :
                                       QueueMode.QUEUE,
        new TtsSpeechProperties({endCallback: opt_speakCallback}));
    ChromeVox.braille.write(new NavBraille({text: new Spannable(text)}));
    LearnMode.shouldFlushSpeech_ = false;
  }

  /** Clears ChromeVox range. */
  static async clearRange() {
    await BackgroundBridge.ChromeVoxState.clearCurrentRange();
  }

  /** @private */
  static resetListeners_() {
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
    BackgroundBridge.BrailleCommandHandler.setEnabled(true);
    chrome.runtime.sendMessage(
        {target: 'GestureCommandHandler', action: 'setEnabled', value: true});
  }

  /** @private */
  static maybeClose_() {
    // Reset listeners and close this page if we somehow move outside of the
    // explorer window.
    chrome.windows.getLastFocused({populate: true}, focusedWindow => {
      if (focusedWindow && focusedWindow.focused &&
          focusedWindow.tabs.find(tab => {
            return tab.url === location.href;
          })) {
        return;
      }

      LearnMode.close_();
    });
  }

  /** @private */
  static close_() {
    LearnMode.output(Msgs.getMsg('learn_mode_outtro'));
    LearnMode.resetListeners_();
    window.close();
  }
}

/**
 * Indicates when speech output should flush previous speech.
 * @private {boolean}
 */
LearnMode.shouldFlushSpeech_ = false;

/**
 * Last time a touch explore gesture was described.
 * @private {!Date}
 */
LearnMode.lastTouchExplore_ = new Date();

/**
 * The minimum time to wait before describing another touch explore gesture.
 * @private {number}
 */
LearnMode.MIN_TOUCH_EXPLORE_OUTPUT_TIME_MS_ = 1000;

document.addEventListener('DOMContentLoaded', function() {
  LearnMode.init();
}, false);

/**
 * Shortcut for document.getElementById.
 * @param {string} id of the element.
 * @return {Element} with the id.
 */
function $(id) {
  return document.getElementById(id);
}
