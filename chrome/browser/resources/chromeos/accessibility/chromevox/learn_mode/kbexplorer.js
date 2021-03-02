// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script for ChromeOS keyboard explorer.
 *
 */

goog.provide('KbExplorer');

goog.require('BrailleCommandData');
goog.require('GestureCommandData');
goog.require('Spannable');
goog.require('AbstractTts');
goog.require('BrailleKeyCommand');
goog.require('ChromeVoxKbHandler');
goog.require('CommandStore');
goog.require('KeyMap');
goog.require('KeyUtil');
goog.require('LibLouis');
goog.require('NavBraille');

/**
 * Class to manage the keyboard explorer.
 */
KbExplorer = class {
  constructor() {}

  /**
   * Initialize keyboard explorer.
   */
  static init() {
    // Export global objects from the background page context into this one.
    window.backgroundWindow = chrome.extension.getBackgroundPage();
    window.ChromeVox = window.backgroundWindow['ChromeVox'];

    window.backgroundWindow.addEventListener(
        'keydown', KbExplorer.onKeyDown, true);
    window.backgroundWindow.addEventListener('keyup', KbExplorer.onKeyUp, true);
    window.backgroundWindow.addEventListener(
        'keypress', KbExplorer.onKeyPress, true);
    chrome.brailleDisplayPrivate.onKeyEvent.addListener(
        KbExplorer.onBrailleKeyEvent);
    chrome.accessibilityPrivate.onAccessibilityGesture.addListener(
        KbExplorer.onAccessibilityGesture);
    chrome.accessibilityPrivate.setKeyboardListener(true, true);
    window.backgroundWindow['BrailleCommandHandler']['setEnabled'](false);
    window.backgroundWindow['GestureCommandHandler']['setEnabled'](false);

    ChromeVoxKbHandler.handlerKeyMap = KeyMap.get();

    /** @type {LibLouis.Translator} */
    KbExplorer.currentBrailleTranslator_ =
        window
            .backgroundWindow['BrailleBackground']['getInstance']()['getTranslatorManager']()['getDefaultTranslator']();

    ChromeVoxKbHandler.commandHandler = KbExplorer.onCommand;

    $('instruction').textContent = Msgs.getMsg('learn_mode_intro');
    KbExplorer.shouldFlushSpeech_ = true;
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
      KbExplorer.output(KeyUtil.getReadableNameForKeyCode(evt.keyCode));

      // Allow Ctrl+W or escape to be handled.
      if ((evt.key === 'w' && evt.ctrlKey) || evt.key === 'Escape') {
        KbExplorer.close_();
        return true;
      }

      ChromeVoxKbHandler.basicKeyDownActionsListener(evt);
      KbExplorer.clearRange();
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
    KbExplorer.shouldFlushSpeech_ = true;
    KbExplorer.maybeClose_();
    KbExplorer.clearRange();
    evt.preventDefault();
    evt.stopPropagation();
  }

  /**
   * Handles keypress events.
   * @param {Event} evt key event.
   */
  static onKeyPress(evt) {
    KbExplorer.clearRange();
    evt.preventDefault();
    evt.stopPropagation();
  }

  /**
   * @param {BrailleKeyEvent} evt The key event.
   */
  static onBrailleKeyEvent(evt) {
    KbExplorer.shouldFlushSpeech_ = true;
    KbExplorer.maybeClose_();
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
            callback = KbExplorer.close_;
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
        if (command && KbExplorer.onCommand(command)) {
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
        KbExplorer.currentBrailleTranslator_.backTranslate(
            cells, function(res) {
              KbExplorer.output(res);
            }.bind(this));
      }
        return;
      case BrailleKeyCommand.STANDARD_KEY:
        break;
    }
    if (msgid) {
      text = Msgs.getMsg(msgid, msgArgs);
    }
    KbExplorer.output(text || evt.command, callback);
    KbExplorer.clearRange();
  }

  /**
   * Handles accessibility gestures from the touch screen.
   * @param {string} gesture The gesture to handle, based on the
   *     ax::mojom::Gesture enum defined in ui/accessibility/ax_enums.mojom
   */
  static onAccessibilityGesture(gesture) {
    KbExplorer.shouldFlushSpeech_ = true;
    KbExplorer.maybeClose_();

    let callback;
    if (gesture === chrome.accessibilityPrivate.Gesture.TOUCH_EXPLORE) {
      if ((new Date() - KbExplorer.lastTouchExplore_) <
          KbExplorer.MIN_TOUCH_EXPLORE_OUTPUT_TIME_MS_) {
        return;
      }
      KbExplorer.lastTouchExplore_ = new Date();
    } else if (gesture === chrome.accessibilityPrivate.Gesture.SWIPE_LEFT2) {
      callback = KbExplorer.close_;
    }

    const gestureData = GestureCommandData.GESTURE_COMMAND_MAP[gesture];
    if (gestureData) {
      if (gestureData.msgId) {
        KbExplorer.output(Msgs.getMsg(gestureData.msgId));
      }
      if (gestureData.command) {
        KbExplorer.onCommand(gestureData.command);
      }
      if (gestureData.commandDescriptionMsgId) {
        KbExplorer.output(
            Msgs.getMsg(gestureData.commandDescriptionMsgId), callback);
      }
    }
  }

  /**
   * Queues up command description.
   * @param {string} command
   * @return {boolean|undefined} True if command existed and was handled.
   */
  static onCommand(command) {
    const msg = CommandStore.messageForCommand(command);
    if (msg) {
      const commandText = Msgs.getMsg(msg);
      KbExplorer.output(commandText);
      KbExplorer.clearRange();
      return true;
    }
  }

  /**
   * @param {string} text
   * @param {function()=} opt_speakCallback A callback to run when speech
   *     finishes.
   */
  static output(text, opt_speakCallback) {
    ChromeVox.tts.speak(
        text,
        KbExplorer.shouldFlushSpeech_ ?
            window.backgroundWindow.QueueMode.FLUSH :
            window.backgroundWindow.QueueMode.QUEUE,
        {endCallback: opt_speakCallback});
    ChromeVox.braille.write(new NavBraille({text: new Spannable(text)}));
    KbExplorer.shouldFlushSpeech_ = false;
  }

  /** Clears ChromeVox range. */
  static clearRange() {
    chrome.extension
        .getBackgroundPage()['ChromeVoxState']['instance']['setCurrentRange'](
            null);
  }

  /** @private */
  static resetListeners_() {
    window.backgroundWindow.removeEventListener(
        'keydown', KbExplorer.onKeyDown, true);
    window.backgroundWindow.removeEventListener(
        'keyup', KbExplorer.onKeyUp, true);
    window.backgroundWindow.removeEventListener(
        'keypress', KbExplorer.onKeyPress, true);
    chrome.brailleDisplayPrivate.onKeyEvent.removeListener(
        KbExplorer.onBrailleKeyEvent);
    chrome.accessibilityPrivate.onAccessibilityGesture.removeListener(
        KbExplorer.onAccessibilityGesture);
    chrome.accessibilityPrivate.setKeyboardListener(true, false);
    window.backgroundWindow['BrailleCommandHandler']['setEnabled'](true);
    window.backgroundWindow['GestureCommandHandler']['setEnabled'](true);
  }

  /** @private */
  static maybeClose_() {
    // Reset listeners and close this page if we somehow move outside of the
    // explorer window.
    chrome.windows.getLastFocused({populate: true}, (focusedWindow) => {
      if (focusedWindow && focusedWindow.focused &&
          focusedWindow.tabs.find((tab) => {
            return tab.url === location.href;
          })) {
        return;
      }

      KbExplorer.close_();
    });
  }

  /** @private */
  static close_() {
    KbExplorer.output(Msgs.getMsg('learn_mode_outtro'));
    KbExplorer.resetListeners_();
    window.close();
  }
};

/**
 * Indicates when speech output should flush previous speech.
 * @private {boolean}
 */
KbExplorer.shouldFlushSpeech_ = false;

/**
 * Last time a touch explore gesture was described.
 * @private {!Date}
 */
KbExplorer.lastTouchExplore_ = new Date();

/**
 * The minimum time to wait before describing another touch explore gesture.
 * @private {number}
 */
KbExplorer.MIN_TOUCH_EXPLORE_OUTPUT_TIME_MS_ = 1000;
