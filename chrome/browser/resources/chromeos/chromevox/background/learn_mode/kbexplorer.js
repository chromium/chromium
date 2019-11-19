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

/**
 * Class to manage the keyboard explorer.
 * @constructor
 */
KbExplorer = function() {};


/**
 * Initialize keyboard explorer.
 */
KbExplorer.init = function() {
  var backgroundWindow = chrome.extension.getBackgroundPage();
  backgroundWindow.addEventListener('keydown', KbExplorer.onKeyDown, true);
  backgroundWindow.addEventListener('keyup', KbExplorer.onKeyUp, true);
  backgroundWindow.addEventListener('keypress', KbExplorer.onKeyPress, true);
  chrome.brailleDisplayPrivate.onKeyEvent.addListener(
      KbExplorer.onBrailleKeyEvent);
  chrome.accessibilityPrivate.onAccessibilityGesture.addListener(
      KbExplorer.onAccessibilityGesture);
  chrome.accessibilityPrivate.setKeyboardListener(true, true);
  backgroundWindow['BrailleCommandHandler']['setEnabled'](false);
  backgroundWindow['GestureCommandHandler']['setEnabled'](false);

  if (localStorage['useClassic'] != 'true') {
    ChromeVoxKbHandler.handlerKeyMap = KeyMap.fromNext();
    ChromeVox.modKeyStr = 'Search';
  } else {
    ChromeVoxKbHandler.handlerKeyMap = KeyMap.fromDefaults();
    ChromeVox.modKeyStr = 'Search+Shift';
  }

  /** @type {LibLouis.Translator} */
  KbExplorer.currentBrailleTranslator_ =
      backgroundWindow['BrailleBackground']['getInstance']()['getTranslatorManager']()['getDefaultTranslator']();

  ChromeVoxKbHandler.commandHandler = KbExplorer.onCommand;
  $('instruction').focus();

  KbExplorer.output(Msgs.getMsg('learn_mode_intro'));
};


/**
 * Handles keydown events by speaking the human understandable name of the key.
 * @param {Event} evt key event.
 * @return {boolean} True if the default action should be performed.
 */
KbExplorer.onKeyDown = function(evt) {
  chrome.extension.getBackgroundPage()['speak'](
      KeyUtil.getReadableNameForKeyCode(evt.keyCode), false,
      AbstractTts.PERSONALITY_ANNOTATION);

  // Allow Ctrl+W or escape to be handled.
  if ((evt.key == 'w' && evt.ctrlKey) || evt.key == 'Escape') {
    KbExplorer.close_();
    return true;
  }

  ChromeVoxKbHandler.basicKeyDownActionsListener(evt);
  KbExplorer.clearRange();
  evt.preventDefault();
  evt.stopPropagation();
  return false;
};


/**
 * Handles keyup events.
 * @param {Event} evt key event.
 */
KbExplorer.onKeyUp = function(evt) {
  KbExplorer.maybeClose_();
  KbExplorer.clearRange();
  evt.preventDefault();
  evt.stopPropagation();
};


/**
 * Handles keypress events.
 * @param {Event} evt key event.
 */
KbExplorer.onKeyPress = function(evt) {
  KbExplorer.clearRange();
  evt.preventDefault();
  evt.stopPropagation();
};

/**
 * @param {BrailleKeyEvent} evt The key event.
 */
KbExplorer.onBrailleKeyEvent = function(evt) {
  KbExplorer.maybeClose_();
  var msgid;
  var msgArgs = [];
  var text;
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
      var dots = evt.brailleDots;
      if (!dots) {
        return;
      }

      // First, check for the dots mapping to a key code.
      var keyCode = BrailleKeyEvent.brailleChordsToStandardKeyCode[dots];
      if (keyCode) {
        text = keyCode;
        break;
      }

      // Next, check for the modifier mappings.
      var mods = BrailleKeyEvent.brailleDotsToModifiers[dots];
      if (mods) {
        var outputs = [];
        for (var mod in mods) {
          if (mod == 'ctrlKey') {
            outputs.push('control');
          } else if (mod == 'altKey') {
            outputs.push('alt');
          } else if (mod == 'shiftKey') {
            outputs.push('shift');
          }
        }

        text = outputs.join(' ');
        break;
      }

      var command = BrailleCommandData.getCommand(dots);
      if (command && KbExplorer.onCommand(command)) {
        return;
      }
      text = BrailleCommandData.makeShortcutText(dots, true);
      break;
    case BrailleKeyCommand.DOTS:
      var dots = evt.brailleDots;
      if (!dots) {
        return;
      }
      var cells = new ArrayBuffer(1);
      var view = new Uint8Array(cells);
      view[0] = dots;
      KbExplorer.currentBrailleTranslator_.backTranslate(cells, function(res) {
        KbExplorer.output(res);
      }.bind(this));
      return;
    case BrailleKeyCommand.STANDARD_KEY:
      break;
  }
  if (msgid) {
    text = Msgs.getMsg(msgid, msgArgs);
  }
  KbExplorer.output(text || evt.command);
  KbExplorer.clearRange();
};

/**
 * Handles accessibility gestures from the touch screen.
 * @param {string} gesture The gesture to handle, based on the
 *     ax::mojom::Gesture enum defined in ui/accessibility/ax_enums.mojom
 */
KbExplorer.onAccessibilityGesture = function(gesture) {
  KbExplorer.maybeClose_();
  var gestureData = GestureCommandData.GESTURE_COMMAND_MAP[gesture];
  if (gestureData) {
    KbExplorer.onCommand(gestureData.command);
  }
};

/**
 * Queues up command description.
 * @param {string} command
 * @return {boolean|undefined} True if command existed and was handled.
 */
KbExplorer.onCommand = function(command) {
  var msg = CommandStore.messageForCommand(command);
  if (msg) {
    var commandText = Msgs.getMsg(msg);
    KbExplorer.output(commandText);
    KbExplorer.clearRange();
    return true;
  }
};

/**
 * @param {string} text
 * @param {string=} opt_braille If different from text.
 */
KbExplorer.output = function(text, opt_braille) {
  chrome.extension.getBackgroundPage()['speak'](text);
  chrome.extension.getBackgroundPage().ChromeVox.braille.write(
      {text: new Spannable(opt_braille || text)});
};

/** Clears ChromeVox range. */
KbExplorer.clearRange = function() {
  chrome.extension
      .getBackgroundPage()['ChromeVoxState']['instance']['setCurrentRange'](
          null);
};

/** @private */
KbExplorer.resetListeners_ = function() {
  var backgroundWindow = chrome.extension.getBackgroundPage();
  backgroundWindow.removeEventListener('keydown', KbExplorer.onKeyDown, true);
  backgroundWindow.removeEventListener('keyup', KbExplorer.onKeyUp, true);
  backgroundWindow.removeEventListener('keypress', KbExplorer.onKeyPress, true);
  chrome.brailleDisplayPrivate.onKeyEvent.removeListener(
      KbExplorer.onBrailleKeyEvent);
  chrome.accessibilityPrivate.onAccessibilityGesture.removeListener(
      KbExplorer.onAccessibilityGesture);
  chrome.accessibilityPrivate.setKeyboardListener(true, false);
  backgroundWindow['BrailleCommandHandler']['setEnabled'](true);
  backgroundWindow['GestureCommandHandler']['setEnabled'](true);
};

/** @private */
KbExplorer.maybeClose_ = function() {
  // Reset listeners and close this page if we somehow move outside of the
  // explorer window.
  chrome.windows.getLastFocused({populate: true}, (focusedWindow) => {
    if (focusedWindow && focusedWindow.focused &&
        focusedWindow.tabs.find((tab) => {
          return tab.url == location.href;
        }))
      return;

    KbExplorer.close_();
  });
};

/** @private */
KbExplorer.close_ = function() {
  KbExplorer.output(Msgs.getMsg('learn_mode_outtro'));
  KbExplorer.resetListeners_();
  window.close();
};
