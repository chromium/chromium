// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A collection of JavaScript utilities used to simplify working
 * with keyboard events.
 */
import {AsyncUtil} from '../../common/async_util.js';
import {KeyCode, KeyName} from '../../common/key_code.js';

import {KeySequence} from './key_sequence.js';
import {Msgs} from './msgs.js';

export class KeyUtil {
  /**
   * Convert a key event into a Key Sequence representation.
   *
   * @param {Event} keyEvent The keyEvent to convert.
   * @return {!KeySequence} A key sequence representation of the key event.
   */
  static keyEventToKeySequence(keyEvent) {
    const util = KeyUtil;
    if (util.prevKeySequence &&
        (util.maxSeqLength === util.prevKeySequence.length())) {
      // Reset the sequence buffer if max sequence length is reached.
      util.sequencing = false;
      util.prevKeySequence = null;
    }
    // Either we are in the middle of a key sequence (N > H), or the key prefix
    // was pressed before (Ctrl+Z), or sticky mode is enabled
    const keyIsPrefixed =
        util.sequencing || keyEvent['keyPrefix'] || keyEvent['stickyMode'];

    // Create key sequence.
    let keySequence = new KeySequence(keyEvent);

    // Check if the Cvox key should be considered as pressed because the
    // modifier key combination is active.
    const keyWasCvox = keySequence.cvoxModifier;

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
    const currTime = new Date().getTime();
    if (KeyUtil.isDoubleTapKey(keySequence) && util.prevKeySequence &&
        keySequence.equals(util.prevKeySequence)) {
      const prevTime = util.modeKeyPressTime;
      const delta = currTime - prevTime;
      if (!keyEvent.repeat && prevTime > 0 && delta < 300) /* Double tap */ {
        keySequence = util.prevKeySequence;
        keySequence.doubleTap = true;
        util.prevKeySequence = null;
        util.sequencing = false;
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
  }

  /**
   * Returns the string representation of the specified key code.
   *
   * @param {number} keyCode key code.
   * @return {string} A string representation of the key event.
   */
  static keyCodeToString(keyCode) {
    if (keyCode === KeyCode.CONTROL) {
      return 'Ctrl';
    }
    if (KeyName.fromCode(keyCode)) {
      return KeyName.fromCode(keyCode);
    }

    // Anything else
    return '#' + keyCode;
  }

  /**
   * Returns the keycode of a string representation of the specified modifier.
   *
   * @param {string} keyString Modifier key.
   * @return {number} Key code.
   */
  static modStringToKeyCode(keyString) {
    switch (keyString) {
      case 'Ctrl':
        return KeyCode.CONTROL;
      case 'Alt':
        return KeyCode.ALT;
      case 'Shift':
        return KeyCode.SHIFT;
      case 'Cmd':
      case 'Win':
        return KeyCode.SEARCH;
    }
    return -1;
  }

  /**
   * Returns the key codes of a string respresentation of the ChromeVox
   * modifiers.
   *
   * @return {Array<number>} Array of key codes.
   */
  static cvoxModKeyCodes() {
    const modKeyCombo = KeySequence.modKeyStr.split(/\+/g);
    const modKeyCodes =
        modKeyCombo.map(keyString => KeyUtil.modStringToKeyCode(keyString));
    return modKeyCodes;
  }

  /**
   * Checks if the specified key code is a key used for switching into a
   * sequence mode. Sequence switch keys are specified in
   * KeySequence.sequenceSwitchKeyCodes
   *
   * @param {!KeySequence} rhKeySeq The key sequence to check.
   * @return {boolean} true if it is a sequence switch keycode, false otherwise.
   */
  static isSequenceSwitchKeyCode(rhKeySeq) {
    for (let i = 0; i < KeySequence.sequenceSwitchKeyCodes.length; i++) {
      const lhKeySeq = KeySequence.sequenceSwitchKeyCodes[i];
      if (lhKeySeq.equals(rhKeySeq)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Get readable string description of the specified keycode.
   *
   * @param {number} keyCode The key code.
   * @return {string} Returns a string description.
   */
  static getReadableNameForKeyCode(keyCode) {
    const msg = Msgs.getMsg.bind(Msgs);
    switch (keyCode) {
      case KeyCode.BROWSER_BACK:
        return msg('back_key');
      case KeyCode.BROWSER_FORWARD:
        return msg('forward_key');
      case KeyCode.BROWSER_REFRESH:
        return msg('refresh_key');
      case KeyCode.ZOOM:
        return msg('toggle_full_screen_key');
      case KeyCode.MEDIA_LAUNCH_APP1:
        return msg('window_overview_key');
      case KeyCode.BRIGHTNESS_DOWN:
        return msg('brightness_down_key');
      case KeyCode.BRIGHTNESS_UP:
        return msg('brightness_up_key');
      case KeyCode.VOLUME_MUTE:
        return msg('volume_mute_key');
      case KeyCode.VOLUME_DOWN:
        return msg('volume_down_key');
      case KeyCode.VOLUME_UP:
        return msg('volume_up_key');
      case KeyCode.ASSISTANT:
        return msg('assistant_key');
      case KeyCode.MEDIA_PLAY_PAUSE:
        return msg('media_play_pause');
    }
    return KeyName.fromCode(keyCode);
  }

  /**
   * Get the platform specific sticky key keycode.
   *
   * @return {number} The platform specific sticky key keycode.
   */
  static getStickyKeyCode() {
    return KeyCode.SEARCH;  // Search.
  }

  /**
   * Get readable string description for an internal string representation of a
   * key or a keyboard shortcut.
   *
   * @param {string} keyStr The internal string repsentation of a key or
   *     a keyboard shortcut.
   * @return {?string} Readable string representation of the input.
   */
  static getReadableNameForStr(keyStr) {
    // TODO (clchen): Refactor this function away since it is no longer used.
    return null;
  }

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
   * @param {boolean=} opt_modifiers Restrict printout to only modifiers.
   *     Defaults
   * to false.
   */
  static async keySequenceToString(
      keySequence, opt_readableKeyCode, opt_modifiers) {
    // TODO(rshearer): Move this method and the getReadableNameForKeyCode and
    // the method to KeySequence after we refactor isModifierActive (when the
    // modifie key becomes customizable and isn't stored as a string). We can't
    // do it earlier because isModifierActive uses
    // KeyUtil.getReadableNameForKeyCode, and I don't want KeySequence to depend
    // on KeyUtil.
    let str = '';

    const numKeys = keySequence.length();

    for (let index = 0; index < numKeys; index++) {
      if (str !== '' && !opt_modifiers) {
        str += ', then ';
      } else if (str !== '') {
        str += '+';
      }

      // This iterates through the sequence. Either we're on the first key
      // pressed or the second
      let tempStr = '';
      for (const keyPressed in keySequence.keys) {
        // This iterates through the actual key, taking into account any
        // modifiers.
        if (!keySequence.keys[keyPressed][index]) {
          continue;
        }
        let modifier = '';
        switch (keyPressed) {
          case 'ctrlKey':
            // TODO(rshearer): This is a hack to work around the special casing
            // of the Ctrl key that used to happen in keyEventToString. We won't
            // need it once we move away from strings completely.
            modifier = 'Ctrl';
            break;
          case 'searchKeyHeld':
            const searchKey = KeyUtil.getReadableNameForKeyCode(KeyCode.SEARCH);
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
            const metaKey = KeyUtil.getReadableNameForKeyCode(KeyCode.SEARCH);
            modifier = metaKey;
            break;
          case 'keyCode':
            const keyCode = keySequence.keys[keyPressed][index];
            // We make sure the keyCode isn't for a modifier key. If it is, then
            // we've already added that into the string above.
            if (keySequence.isModifierKey(keyCode) || opt_modifiers) {
              break;
            }

            if (!opt_readableKeyCode) {
              tempStr += KeyUtil.keyCodeToString(keyCode);
              break;
            }

            // First, try using Chrome OS's localized DOM key string conversion.
            let domKeyString =
                await AsyncUtil.getLocalizedDomKeyStringForKeyCode(keyCode);
            if (!domKeyString) {
              tempStr += KeyUtil.getReadableNameForKeyCode(keyCode);
              break;
            }

            // Upper case single-lettered key strings for better tts.
            if (domKeyString.length === 1) {
              domKeyString = domKeyString.toUpperCase();
            }

            tempStr += domKeyString;
            break;
        }
        if (str.indexOf(modifier) === -1) {
          tempStr += modifier + '+';
        }
      }
      str += tempStr;

      // Strip trailing +.
      if (str[str.length - 1] === '+') {
        str = str.slice(0, -1);
      }
    }

    if (keySequence.cvoxModifier || keySequence.prefixKey) {
      if (str !== '') {
        str = 'Search+' + str;
      } else {
        str = 'Search+Search';
      }
    } else if (keySequence.stickyMode) {
      // Strip trailing ', then '.
      const cut = str.slice(str.length - ', then '.length);
      if (cut === ', then ') {
        str = str.slice(0, str.length - cut.length);
      }
      str = str + '+' + str;
    }
    return str;
  }

  /**
   * Looks up if the given key sequence is triggered via double tap.
   * @param {KeySequence} key The key.
   * @return {boolean} True if key is triggered via double tap.
   */
  static isDoubleTapKey(key) {
    let isSet = false;
    const originalState = key.doubleTap;
    key.doubleTap = true;
    for (let i = 0, keySeq; keySeq = KeySequence.doubleTapCache[i]; i++) {
      if (keySeq.equals(key)) {
        isSet = true;
        break;
      }
    }
    key.doubleTap = originalState;
    return isSet;
  }
}

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
