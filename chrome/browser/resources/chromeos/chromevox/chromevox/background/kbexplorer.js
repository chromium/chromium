// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script for ChromeOS keyboard explorer.
 *
 */

goog.provide('cvox.KbExplorer');

goog.require('BrailleCommandData');
goog.require('GestureCommandData');
goog.require('Spannable');
goog.require('cvox.AbstractTts');
goog.require('cvox.BrailleKeyCommand');
goog.require('cvox.ChromeVoxKbHandler');
goog.require('cvox.CommandStore');
goog.require('cvox.KeyMap');
goog.require('cvox.KeyUtil');
goog.require('cvox.LibLouis');

/**
 * Class to manage the keyboard explorer.
 * @constructor
 */
cvox.KbExplorer = function() {};


/**
 * Initialize keyboard explorer.
 */
cvox.KbExplorer.init = function() {
  var backgroundWindow = chrome.extension.getBackgroundPage();
  backgroundWindow.addEventListener('keydown', cvox.KbExplorer.onKeyDown, true);
  backgroundWindow.addEventListener('keyup', cvox.KbExplorer.onKeyUp, true);
  backgroundWindow.addEventListener(
      'keypress', cvox.KbExplorer.onKeyPress, true);
  chrome.brailleDisplayPrivate.onKeyEvent.addListener(
      cvox.KbExplorer.onBrailleKeyEvent);
  chrome.accessibilityPrivate.onAccessibilityGesture.addListener(
      cvox.KbExplorer.onAccessibilityGesture);
  chrome.accessibilityPrivate.setKeyboardListener(true, true);
  backgroundWindow['BrailleCommandHandler']['setEnabled'](false);
  backgroundWindow['GestureCommandHandler']['setEnabled'](false);

  if (localStorage['useClassic'] != 'true') {
    cvox.ChromeVoxKbHandler.handlerKeyMap = cvox.KeyMap.fromNext();
    cvox.ChromeVox.modKeyStr = 'Search';
  } else {
    cvox.ChromeVoxKbHandler.handlerKeyMap = cvox.KeyMap.fromDefaults();
    cvox.ChromeVox.modKeyStr = 'Search+Shift';
  }

  /** @type {cvox.LibLouis.Translator} */
  cvox.KbExplorer.currentBrailleTranslator_ =
      backgroundWindow['cvox']['BrailleBackground']['getInstance']()
          ['getTranslatorManager']()['getDefaultTranslator']();

  cvox.ChromeVoxKbHandler.commandHandler = cvox.KbExplorer.onCommand;
  $('instruction').focus();
};


/**
 * Handles keydown events by speaking the human understandable name of the key.
 * @param {Event} evt key event.
 * @return {boolean} True if the default action should be performed.
 */
cvox.KbExplorer.onKeyDown = function(evt) {
  chrome.extension.getBackgroundPage()['speak'](
      cvox.KeyUtil.getReadableNameForKeyCode(evt.keyCode), false,
      cvox.AbstractTts.PERSONALITY_ANNOTATION);

  // Allow Ctrl+W or escape to be handled.
  if ((evt.key == 'w' && evt.ctrlKey) || evt.key == 'Escape') {
    cvox.KbExplorer.resetListeners_();
    window.close();
    return true;
  }

  cvox.ChromeVoxKbHandler.basicKeyDownActionsListener(evt);
  cvox.KbExplorer.clearRange();
  evt.preventDefault();
  evt.stopPropagation();
  return false;
};


/**
 * Handles keyup events.
 * @param {Event} evt key event.
 */
cvox.KbExplorer.onKeyUp = function(evt) {
  cvox.KbExplorer.clearRange();
  evt.preventDefault();
  evt.stopPropagation();
};


/**
 * Handles keypress events.
 * @param {Event} evt key event.
 */
cvox.KbExplorer.onKeyPress = function(evt) {
  cvox.KbExplorer.clearRange();
  evt.preventDefault();
  evt.stopPropagation();
};

/**
 * @param {cvox.BrailleKeyEvent} evt The key event.
 */
cvox.KbExplorer.onBrailleKeyEvent = function(evt) {
  var msgid;
  var msgArgs = [];
  var text;
  switch (evt.command) {
    case cvox.BrailleKeyCommand.PAN_LEFT:
      msgid = 'braille_pan_left';
      break;
    case cvox.BrailleKeyCommand.PAN_RIGHT:
      msgid = 'braille_pan_right';
      break;
    case cvox.BrailleKeyCommand.LINE_UP:
      msgid = 'braille_line_up';
      break;
    case cvox.BrailleKeyCommand.LINE_DOWN:
      msgid = 'braille_line_down';
      break;
    case cvox.BrailleKeyCommand.TOP:
      msgid = 'braille_top';
      break;
    case cvox.BrailleKeyCommand.BOTTOM:
      msgid = 'braille_bottom';
      break;
    case cvox.BrailleKeyCommand.ROUTING:
    case cvox.BrailleKeyCommand.SECONDARY_ROUTING:
      msgid = 'braille_routing';
      msgArgs.push(/** @type {number} */ (evt.displayPosition + 1));
      break;
    case cvox.BrailleKeyCommand.CHORD:
      var dots = evt.brailleDots;
      if (!dots)
        return;

      // First, check for the dots mapping to a key code.
      var keyCode = cvox.BrailleKeyEvent.brailleChordsToStandardKeyCode[dots];
      if (keyCode) {
        text = keyCode;
        break;
      }

      // Next, check for the modifier mappings.
      var mods = cvox.BrailleKeyEvent.brailleDotsToModifiers[dots];
      if (mods) {
        var outputs = [];
        for (var mod in mods) {
          if (mod == 'ctrlKey')
            outputs.push('control');
          else if (mod == 'altKey')
            outputs.push('alt');
          else if (mod == 'shiftKey')
            outputs.push('shift');
        }

        text = outputs.join(' ');
        break;
      }

      var command = BrailleCommandData.getCommand(dots);
      if (command && cvox.KbExplorer.onCommand(command))
        return;
      text = BrailleCommandData.makeShortcutText(dots, true);
      break;
    case cvox.BrailleKeyCommand.DOTS:
      var dots = evt.brailleDots;
      if (!dots)
        return;
      var cells = new ArrayBuffer(1);
      var view = new Uint8Array(cells);
      view[0] = dots;
      cvox.KbExplorer.currentBrailleTranslator_.backTranslate(
          cells, function(res) {
            cvox.KbExplorer.output(res);
          }.bind(this));
      return;
    case cvox.BrailleKeyCommand.STANDARD_KEY:
      break;
  }
  if (msgid)
    text = Msgs.getMsg(msgid, msgArgs);
  cvox.KbExplorer.output(text || evt.command);
  cvox.KbExplorer.clearRange();
};

/**
 * Handles accessibility gestures from the touch screen.
 * @param {string} gesture The gesture to handle, based on the ax::mojom::Gesture enum
 *     defined in ui/accessibility/ax_enums.idl
 */
cvox.KbExplorer.onAccessibilityGesture = function(gesture) {
  var gestureData = GestureCommandData.GESTURE_COMMAND_MAP[gesture];
  if (gestureData)
    cvox.KbExplorer.onCommand(gestureData.command);
};

/**
 * Queues up command description.
 * @param {string} command
 * @return {boolean|undefined} True if command existed and was handled.
 */
cvox.KbExplorer.onCommand = function(command) {
  var msg = cvox.CommandStore.messageForCommand(command);
  if (msg) {
    var commandText = Msgs.getMsg(msg);
    cvox.KbExplorer.output(commandText);
    cvox.KbExplorer.clearRange();
    return true;
  }
};

/**
 * @param {string} text
 * @param {string=} opt_braille If different from text.
 */
cvox.KbExplorer.output = function(text, opt_braille) {
  chrome.extension.getBackgroundPage()['speak'](text);
  chrome.extension.getBackgroundPage().cvox.ChromeVox.braille.write(
      {text: new Spannable(opt_braille || text)});
};

/** Clears ChromeVox range. */
cvox.KbExplorer.clearRange = function() {
  chrome.extension.getBackgroundPage()['ChromeVoxState']['instance']
                                      ['setCurrentRange'](null);
};

/** @private */
cvox.KbExplorer.resetListeners_ = function() {
  var backgroundWindow = chrome.extension.getBackgroundPage();
  backgroundWindow.removeEventListener(
      'keydown', cvox.KbExplorer.onKeyDown, true);
  backgroundWindow.removeEventListener('keyup', cvox.KbExplorer.onKeyUp, true);
  backgroundWindow.removeEventListener(
      'keypress', cvox.KbExplorer.onKeyPress, true);
  chrome.brailleDisplayPrivate.onKeyEvent.removeListener(
      cvox.KbExplorer.onBrailleKeyEvent);
  chrome.accessibilityPrivate.onAccessibilityGesture.removeListener(
      cvox.KbExplorer.onAccessibilityGesture);
  chrome.accessibilityPrivate.setKeyboardListener(true, false);
  backgroundWindow['BrailleCommandHandler']['setEnabled'](true);
  backgroundWindow['GestureCommandHandler']['setEnabled'](true);
};
