// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A collection of JavaScript utilities used to simplify working
 * with keyboard events.
 */


goog.provide('KeyUtil');
goog.provide('SimpleKeyEvent');

goog.require('Msgs');
goog.require('ChromeVox');
goog.require('KeySequence');

/**
 * @typedef {{ctrlKey: (boolean|undefined),
 *            altKey: (boolean|undefined),
 *            shiftKey: (boolean|undefined),
 *            keyCode: (number|undefined)}}
 */
var SimpleKeyEvent;

/**
 * Create the namespace
 * @constructor
 */
KeyUtil = function() {};

/**
 * The time in ms at which the ChromeVox Sticky Mode key was pressed.
 * @type {number}
 */
KeyUtil.modeKeyPressTime = 0;

/**
 * Indicates if sequencing is currently active for building a keyboard shortcut.
 * @type {boolean}
 */
KeyUtil.sequencing = false;

/**
 * The previous KeySequence when sequencing is ON.
 * @type {KeySequence}
 */
KeyUtil.prevKeySequence = null;


/**
 * The sticky key sequence.
 * @type {KeySequence}
 */
KeyUtil.stickyKeySequence = null;

/**
 * Maximum number of key codes the sequence buffer may hold. This is the max
 * length of a sequential keyboard shortcut, i.e. the number of key that can be
 * pressed one after the other while modifier keys (Cros+Shift) are held down.
 * @const
 * @type {number}
 */
KeyUtil.maxSeqLength = 2;


/**
 * Convert a key event into a Key Sequence representation.
 *
 * @param {Event|SimpleKeyEvent} keyEvent The keyEvent to convert.
 * @return {KeySequence} A key sequence representation of the key event.
 */
KeyUtil.keyEventToKeySequence = function(keyEvent) {
  var util = KeyUtil;
  if (util.prevKeySequence &&
      (util.maxSeqLength == util.prevKeySequence.length())) {
    // Reset the sequence buffer if max sequence length is reached.
    util.sequencing = false;
    util.prevKeySequence = null;
  }
  // Either we are in the middle of a key sequence (N > H), or the key prefix
  // was pressed before (Ctrl+Z), or sticky mode is enabled
  var keyIsPrefixed =
      util.sequencing || keyEvent['keyPrefix'] || keyEvent['stickyMode'];

  // Create key sequence.
  var keySequence = new KeySequence(keyEvent);

  // Check if the Cvox key should be considered as pressed because the
  // modifier key combination is active.
  var keyWasCvox = keySequence.cvoxModifier;

  if (keyIsPrefixed || keyWasCvox) {
    if (!util.sequencing && util.isSequenceSwitchKeyCode(keySequence)) {
      // If this is the beginning of a sequence.
      util.sequencing = true;
      util.prevKeySequence = keySequence;
      return keySequence;
    } else if (util.sequencing) {
      if (util.prevKeySequence.addKeyEvent(keyEvent)) {
        keySequence = util.prevKeySequence;
        util.prevKeySequence = null;
        util.sequencing = false;
        return keySequence;
      } else {
        throw 'Think sequencing is enabled, yet util.prevKeySequence already' +
            'has two key codes' + util.prevKeySequence;
      }
    }
  } else {
    util.sequencing = false;
  }

  // Repeated keys pressed.
  var currTime = new Date().getTime();
  if (KeyUtil.isDoubleTapKey(keySequence) && util.prevKeySequence &&
      keySequence.equals(util.prevKeySequence)) {
    var prevTime = util.modeKeyPressTime;
    var delta = currTime - prevTime;
    if (prevTime > 0 && delta > 100 && delta < 300) /* Double tap */ {
      keySequence = util.prevKeySequence;
      keySequence.doubleTap = true;
      util.prevKeySequence = null;
      util.sequencing = false;
      // Resets the search key state tracked for ChromeOS because in OOBE,
      // we never get a key up for the key down (keyCode 91).
      if (keyEvent.keyCode == KeyUtil.getStickyKeyCode()) {
        ChromeVox.searchKeyHeld = false;
      }
      return keySequence;
    }
    // The user double tapped the sticky key but didn't do it within the
    // required time. It's possible they will try again, so keep track of the
    // time the sticky key was pressed and keep track of the corresponding
    // key sequence.
  }
  util.prevKeySequence = keySequence;
  util.modeKeyPressTime = currTime;
  return keySequence;
};

/**
 * Returns the string representation of the specified key code.
 *
 * @param {number} keyCode key code.
 * @return {string} A string representation of the key event.
 */
KeyUtil.keyCodeToString = function(keyCode) {
  if (keyCode == 17) {
    return 'Ctrl';
  }
  if (keyCode == 18) {
    return 'Alt';
  }
  if (keyCode == 16) {
    return 'Shift';
  }
  if ((keyCode == 91) || (keyCode == 93)) {
    return 'Search';
  }
  // TODO(rshearer): This is a hack to work around the special casing of the
  // sticky mode string that used to happen in keyEventToString. We won't need
  // it once we move away from strings completely.
  if (keyCode == 45) {
    return 'Insert';
  }
  if (keyCode >= 65 && keyCode <= 90) {
    // A - Z
    return String.fromCharCode(keyCode);
  } else if (keyCode >= 48 && keyCode <= 57) {
    // 0 - 9
    return String.fromCharCode(keyCode);
  } else {
    // Anything else
    return '#' + keyCode;
  }
};

/**
 * Returns the keycode of a string representation of the specified modifier.
 *
 * @param {string} keyString Modifier key.
 * @return {number} Key code.
 */
KeyUtil.modStringToKeyCode = function(keyString) {
  switch (keyString) {
    case 'Ctrl':
      return 17;
    case 'Alt':
      return 18;
    case 'Shift':
      return 16;
    case 'Cmd':
    case 'Win':
      return 91;
  }
  return -1;
};

/**
 * Returns the key codes of a string respresentation of the ChromeVox modifiers.
 *
 * @return {Array<number>} Array of key codes.
 */
KeyUtil.cvoxModKeyCodes = function() {
  var modKeyCombo = ChromeVox.modKeyStr.split(/\+/g);
  var modKeyCodes = modKeyCombo.map(function(keyString) {
    return KeyUtil.modStringToKeyCode(keyString);
  });
  return modKeyCodes;
};

/**
 * Checks if the specified key code is a key used for switching into a sequence
 * mode. Sequence switch keys are specified in
 * KeyUtil.sequenceSwitchKeyCodes
 *
 * @param {!KeySequence} rhKeySeq The key sequence to check.
 * @return {boolean} true if it is a sequence switch keycode, false otherwise.
 */
KeyUtil.isSequenceSwitchKeyCode = function(rhKeySeq) {
  for (var i = 0; i < ChromeVox.sequenceSwitchKeyCodes.length; i++) {
    var lhKeySeq = ChromeVox.sequenceSwitchKeyCodes[i];
    if (lhKeySeq.equals(rhKeySeq)) {
      return true;
    }
  }
  return false;
};


/**
 * Get readable string description of the specified keycode.
 *
 * @param {number} keyCode The key code.
 * @return {string} Returns a string description.
 */
KeyUtil.getReadableNameForKeyCode = function(keyCode) {
  var msg = Msgs.getMsg.bind(Msgs);
  if (keyCode == 0) {
    return 'Power button';
  } else if (keyCode == 17) {
    return 'Control';
  } else if (keyCode == 18) {
    return 'Alt';
  } else if (keyCode == 16) {
    return 'Shift';
  } else if (keyCode == 9) {
    return 'Tab';
  } else if ((keyCode == 91) || (keyCode == 93)) {
    return 'Search';
  } else if (keyCode == 8) {
    return 'Backspace';
  } else if (keyCode == 32) {
    return 'Space';
  } else if (keyCode == 35) {
    return 'end';
  } else if (keyCode == 36) {
    return 'home';
  } else if (keyCode == 37) {
    return 'Left arrow';
  } else if (keyCode == 38) {
    return 'Up arrow';
  } else if (keyCode == 39) {
    return 'Right arrow';
  } else if (keyCode == 40) {
    return 'Down arrow';
  } else if (keyCode == 45) {
    return 'Insert';
  } else if (keyCode == 13) {
    return 'Enter';
  } else if (keyCode == 27) {
    return 'Escape';
  } else if (keyCode == 112) {
    return msg('back_key');
  } else if (keyCode == 113) {
    return msg('forward_key');
  } else if (keyCode == 114) {
    return msg('refresh_key');
  } else if (keyCode == 115) {
    return msg('toggle_full_screen_key');
  } else if (keyCode == 116) {
    return msg('window_overview_key');
  } else if (keyCode == 117) {
    return msg('brightness_down_key');
  } else if (keyCode == 118) {
    return msg('brightness_up_key');
  } else if (keyCode == 119) {
    return msg('volume_mute_key');
  } else if (keyCode == 120) {
    return msg('volume_down_key');
  } else if (keyCode == 121) {
    return msg('volume_up_key');
  } else if (keyCode == 122) {
    return 'F11';
  } else if (keyCode == 123) {
    return 'F12';
  } else if (keyCode == 153) {
    return msg('assistant_key');
  } else if (keyCode == 186) {
    return 'Semicolon';
  } else if (keyCode == 187) {
    return 'Equal sign';
  } else if (keyCode == 188) {
    return 'Comma';
  } else if (keyCode == 189) {
    return 'Dash';
  } else if (keyCode == 190) {
    return 'Period';
  } else if (keyCode == 191) {
    return 'Forward slash';
  } else if (keyCode == 192) {
    return 'Grave accent';
  } else if (keyCode == 219) {
    return 'Open bracket';
  } else if (keyCode == 220) {
    return 'Back slash';
  } else if (keyCode == 221) {
    return 'Close bracket';
  } else if (keyCode == 222) {
    return 'Single quote';
  } else if (keyCode == 115) {
    return 'Toggle full screen';
  } else if (keyCode >= 48 && keyCode <= 90) {
    return String.fromCharCode(keyCode);
  }
  return '';
};

/**
 * Get the platform specific sticky key keycode.
 *
 * @return {number} The platform specific sticky key keycode.
 */
KeyUtil.getStickyKeyCode = function() {
  return 91;  // Search.
};


/**
 * Get readable string description for an internal string representation of a
 * key or a keyboard shortcut.
 *
 * @param {string} keyStr The internal string repsentation of a key or
 *     a keyboard shortcut.
 * @return {?string} Readable string representation of the input.
 */
KeyUtil.getReadableNameForStr = function(keyStr) {
  // TODO (clchen): Refactor this function away since it is no longer used.
  return null;
};


/**
 * Creates a string representation of a KeySequence.
 * A KeySequence  with a keyCode of 76 ('L') and the control and alt keys down
 * would return the string 'Ctrl+Alt+L', for example. A key code that doesn't
 * correspond to a letter or number will typically return a string with a
 * pound and then its keyCode, like '#39' for Right Arrow. However,
 * if the opt_readableKeyCode option is specified, the key code will return a
 * readable string description like 'Right Arrow' instead of '#39'.
 *
 * The modifiers always come in this order:
 *
 *   Ctrl
 *   Alt
 *   Shift
 *   Meta
 *
 * @param {KeySequence} keySequence The KeySequence object.
 * @param {boolean=} opt_readableKeyCode Whether or not to return a readable
 * string description instead of a string with a pound symbol and a keycode.
 * Default is false.
 * @param {boolean=} opt_modifiers Restrict printout to only modifiers. Defaults
 * to false.
 * @return {string} Readable string representation of the KeySequence object.
 */
KeyUtil.keySequenceToString = function(
    keySequence, opt_readableKeyCode, opt_modifiers) {
  // TODO(rshearer): Move this method and the getReadableNameForKeyCode and the
  // method to KeySequence after we refactor isModifierActive (when the modifie
  // key becomes customizable and isn't stored as a string). We can't do it
  // earlier because isModifierActive uses KeyUtil.getReadableNameForKeyCode,
  // and I don't want KeySequence to depend on KeyUtil.
  var str = '';

  var numKeys = keySequence.length();

  for (var index = 0; index < numKeys; index++) {
    if (str != '' && !opt_modifiers) {
      str += '>';
    } else if (str != '') {
      str += '+';
    }

    // This iterates through the sequence. Either we're on the first key
    // pressed or the second
    var tempStr = '';
    for (var keyPressed in keySequence.keys) {
      // This iterates through the actual key, taking into account any
      // modifiers.
      if (!keySequence.keys[keyPressed][index]) {
        continue;
      }
      var modifier = '';
      switch (keyPressed) {
        case 'ctrlKey':
          // TODO(rshearer): This is a hack to work around the special casing
          // of the Ctrl key that used to happen in keyEventToString. We won't
          // need it once we move away from strings completely.
          modifier = 'Ctrl';
          break;
        case 'searchKeyHeld':
          var searchKey = KeyUtil.getReadableNameForKeyCode(91);
          modifier = searchKey;
          break;
        case 'altKey':
          modifier = 'Alt';
          break;
        case 'altGraphKey':
          modifier = 'AltGraph';
          break;
        case 'shiftKey':
          modifier = 'Shift';
          break;
        case 'metaKey':
          var metaKey = KeyUtil.getReadableNameForKeyCode(91);
          modifier = metaKey;
          break;
        case 'keyCode':
          var keyCode = keySequence.keys[keyPressed][index];
          // We make sure the keyCode isn't for a modifier key. If it is, then
          // we've already added that into the string above.
          if (!keySequence.isModifierKey(keyCode) && !opt_modifiers) {
            if (opt_readableKeyCode) {
              tempStr += KeyUtil.getReadableNameForKeyCode(keyCode);
            } else {
              tempStr += KeyUtil.keyCodeToString(keyCode);
            }
          }
      }
      if (str.indexOf(modifier) == -1) {
        tempStr += modifier + '+';
      }
    }
    str += tempStr;

    // Strip trailing +.
    if (str[str.length - 1] == '+') {
      str = str.slice(0, -1);
    }
  }

  if (keySequence.cvoxModifier || keySequence.prefixKey) {
    if (str != '') {
      str = 'ChromeVox+' + str;
    } else {
      str = 'Cvox';
    }
  } else if (keySequence.stickyMode) {
    if (str[str.length - 1] == '>') {
      str = str.slice(0, -1);
    }
    str = str + '+' + str;
  }
  return str;
};

/**
 * Looks up if the given key sequence is triggered via double tap.
 * @param {KeySequence} key The key.
 * @return {boolean} True if key is triggered via double tap.
 */
KeyUtil.isDoubleTapKey = function(key) {
  var isSet = false;
  var originalState = key.doubleTap;
  key.doubleTap = true;
  for (var i = 0, keySeq; keySeq = KeySequence.doubleTapCache[i]; i++) {
    if (keySeq.equals(key)) {
      isSet = true;
      break;
    }
  }
  key.doubleTap = originalState;
  return isSet;
};
