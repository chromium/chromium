// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview Braille hardware keyboard input method.
 *
 * This method is automatically enabled when a braille display is connected
 * and ChromeVox is turned on.  Most of the braille input and editing logic
 * is located in ChromeVox where the braille translation library is available.
 * This IME connects to ChromeVox and communicates using messages as follows:
 *
 * Sent from this IME to ChromeVox:
 * {type: 'activeState', active: boolean}
 * {type: 'inputContext', context: InputContext}
 *   Sent on focus/blur to inform ChromeVox of the type of the current field.
 *   In the latter case (blur), context is null.
 * {type: 'reset'}
 *   Sent when the {@code onReset} IME event fires or uncommitted text is
 *   committed without being triggered by ChromeVox (e.g. because of a
 *   key press).
 * {type: 'brailleDots', dots: number}
 *   Sent when the user typed a braille cell using the standard keyboard.
 *   ChromeVox treats this similarly to entering braille input using the
 *   braille display.
 * {type: 'backspace', requestId: string}
 *   Sent when the user presses the backspace key.
 *   ChromeVox must respond with a {@code keyEventHandled} message
 *   with the same request id.
 *
 * Sent from ChromeVox to this IME:
 * {type: 'replaceText', contextID: number, deleteBefore: number,
 *  newText: string}
 *   Deletes {@code deleteBefore} characters before the cursor (or selection)
 *   and inserts {@code newText}.  {@code contextID} identifies the text field
 *   to apply the update to (no change will happen if focus has moved to a
 *   different field).
 * {type: 'setUncommitted', contextID: number, text: string}
 *   Stores text for the field identified by contextID to be committed
 *   either as a result of a 'commitUncommitted' message or a by the IME
 *   unhandled key press event.  Unlike 'replaceText', this does not send the
 *   uncommitted text to the input field, but instead stores it in the IME.
 * {type: 'commitUncommitted', contextID: number}
 *   Commits any uncommitted text if it matches the given context ID.
 *   See 'setUncommitted' above.
 * {type: 'keyEventHandled', requestId: string, result: boolean}
 *   Response to a {@code backspace} message indicating whether the
 *   backspace was handled by ChromeVox or should be allowed to propagate
 *   through the normal event handling pipeline.
 */

class BrailleIme {
  constructor() {
    /**
     * Whether to enable extra debug logging for the IME.
     * @const {boolean}
     * @private
     */
    this.DEBUG = false;

    /**
     * ChromeVox extension ID.
     * @const {string}
     * @private
     */
    this.CHROMEVOX_EXTENSION_ID_ = 'mndnfokpggljbaajbnioimlmbfngpief';

    /**
     * Name of the port used for communication with ChromeVox.
     * @const {string}
     * @private
     */
    this.PORT_NAME = 'BrailleIme.Port';

    /**
     * Identifier for the use standard keyboard option used
     * in the menu and
     * {@code localStorage}.  This can be switched on to
     * type braille using the standard keyboard, or off
     * (default) for the usual keyboard behaviour.
     * @const {string}
     */
    this.USE_STANDARD_KEYBOARD_ID = 'useStandardKeyboard';

    /** @private {boolean} */
    this.useStandardKeyboard_ = false;

    /**
     * Braille dots for keys that are currently pressed.
     * @private {number}
     */
    this.pressed_ = 0;

    /**
     * Dots that have been pressed at some point
     * since {@code pressed_} was last
     * {@code 0}.
     * @private {number}
     */
    this.accumulated_ = 0;

    /**
     * Bit in {@code pressed_} and {@code accumulated_} that
     * represent the space key.
     * @const {number}
     */
    this.SPACE = 0x100;

    /**
     * Maps key codes on a standard keyboard to the
     * correspodning dots. Keys on the 'home row' correspond
     * to the keys on a Perkins-style keyboard. Note that
     * the mapping below is arranged like the dots in a
     * braille cell. Only 6 dot input is supported.
     * @private
     * @const {Object<number>}
     */
    this.CODE_TO_DOT_ = {
      'KeyF': 0x01,
      'KeyJ': 0x08,
      'KeyD': 0x02,
      'KeyK': 0x10,
      'KeyS': 0x04,
      'KeyL': 0x20,
      'Space': 0x100,
    };

    /**
     * The current engine ID as set by {@code onActivate}, or the empty string
     * if the IME is not active.
     * @type {string}
     * @private
     */
    this.engineID_ = '';

    /**
     * The port used to communicate with ChromeVox.
     * @type {Port} port_
     * @private
     */
    this.port_ = null;

    /**
     * Uncommitted text and context ID.
     * @type {?{contextID: number, text: string}}
     * @private
     */
    this.uncommitted_ = null;
  }

  /**
   * Registers event listeners in the chrome IME API.
   */
  init() {
    chrome.input.ime.onActivate.addListener(this.onActivate_.bind(this));
    chrome.input.ime.onDeactivated.addListener(this.onDeactivated_.bind(this));
    chrome.input.ime.onFocus.addListener(this.onFocus_.bind(this));
    chrome.input.ime.onBlur.addListener(this.onBlur_.bind(this));
    chrome.input.ime.onInputContextUpdate.addListener(
        this.onInputContextUpdate_.bind(this));
    chrome.input.ime.onKeyEvent.addListener(
        this.onKeyEvent_.bind(this), ['async']);
    chrome.input.ime.onReset.addListener(this.onReset_.bind(this));
    chrome.input.ime.onMenuItemActivated.addListener(
        this.onMenuItemActivated_.bind(this));
    this.connectChromeVox_();
  }

  /**
   * Called by the IME framework when this IME is activated.
   * @param {string} engineID Engine ID, should be 'braille'.
   * @private
   */
  onActivate_(engineID) {
    this.log_('onActivate', engineID);
    this.engineID_ = engineID;
    if (!this.port_) {
      this.connectChromeVox_();
    }
    this.useStandardKeyboard_ =
        localStorage[this.USE_STANDARD_KEYBOARD_ID] === String(true);
    this.accumulated_ = 0;
    this.pressed_ = 0;
    this.updateMenuItems_();
    this.sendActiveState_();
  }

  /**
   * Called by the IME framework when this IME is deactivated.
   * @param {string} engineID Engine ID, should be 'braille'.
   * @private
   */
  onDeactivated_(engineID) {
    this.log_('onDectivated', engineID);
    this.engineID_ = '';
    this.sendActiveState_();
  }

  /**
   * Called by the IME framework when a text field receives focus.
   * @param {chrome.input.ime.InputContext} context Input field context.
   * @private
   */
  onFocus_(context) {
    this.log_('onFocus', context);
    this.sendInputContext_(context);
  }

  /**
   * Called by the IME framework when a text field looses focus.
   * @param {number} contextID Input field context ID.
   * @private
   */
  onBlur_(contextID) {
    this.log_('onBlur', contextID + '');
    this.sendInputContext_(null);
  }

  /**
   * Called by the IME framework when the current input context is updated.
   * @param {chrome.input.ime.InputContext} context Input field context.
   * @private
   */
  onInputContextUpdate_(context) {
    this.log_('onInputContextUpdate', context);
    this.sendInputContext_(context);
  }

  /**
   * Called by the system when this IME is active and a key event is
   * generated.
   * @param {string} engineID Engine ID, should be 'braille'.
   * @param {!ChromeKeyboardEvent} event The keyboard event.
   * @private
   */
  onKeyEvent_(engineID, event) {
    var result = this.processKey_(event);
    if (result !== undefined) {
      this.keyEventHandled_(event.requestId, event.type, result);
    }
  }

  /**
   * Called when chrome ends the current text input session.
   * @param {string} engineID Engine ID, should be 'braille'.
   * @private
   */
  onReset_(engineID) {
    this.log_('onReset', engineID);
    this.engineID_ = engineID;
    this.sendToChromeVox_({type: 'reset'});
  }

  /**
   * Called by the IME framework when a menu item is activated.
   * @param {string} engineID Engine ID, should be 'braille'.
   * @param {string} itemID Identifies the menu item.
   * @private
   */
  onMenuItemActivated_(engineID, itemID) {
    if (engineID === this.engineID_ &&
        itemID === this.USE_STANDARD_KEYBOARD_ID) {
      this.useStandardKeyboard_ = !this.useStandardKeyboard_;
      localStorage[this.USE_STANDARD_KEYBOARD_ID] =
          String(this.useStandardKeyboard_);
      if (!this.useStandardKeyboard_) {
        this.accumulated_ = 0;
        this.pressed_ = 0;
      }
      this.updateMenuItems_();
    }
  }

  /**
   * Outputs a log message to the console, only if {@link BrailleIme.DEBUG}
   * is set to true.
   * @param {string} func Name of the caller.
   * @param {Object|string=} message Message to output.
   * @private
   */
  log_(func, message) {
    if (this.DEBUG) {
      if (typeof (message) !== 'string') {
        message = JSON.stringify(message);
      }
      console.log('BrailleIme.' + func + ': ' + message);
    }
  }

  /**
   * Handles a qwerty key on the home row as a braille key.
   * @param {!ChromeKeyboardEvent} event Keyboard event.
   * @return {boolean|undefined} Whether the event was handled, or
   *     {@code undefined} if handling was delegated to ChromeVox.
   * @private
   */
  processKey_(event) {
    if (!this.useStandardKeyboard_) {
      return false;
    }
    if (event.code === 'Backspace' && event.type === 'keydown') {
      this.pressed_ = 0;
      this.accumulated_ = 0;
      this.sendToChromeVox_({type: 'backspace', requestId: event.requestId});
      return undefined;
    }
    var dot = this.CODE_TO_DOT_[event.code];
    if (!dot || event.altKey || event.ctrlKey || event.shiftKey ||
        event.capsLock) {
      this.pressed_ = 0;
      this.accumulated_ = 0;
      return false;
    }
    if (event.type === 'keydown') {
      this.pressed_ |= dot;
      this.accumulated_ |= this.pressed_;
      return true;
    } else if (event.type === 'keyup') {
      this.pressed_ &= ~dot;
      if (this.pressed_ === 0 && this.accumulated_ !== 0) {
        var dotsToSend = this.accumulated_;
        this.accumulated_ = 0;
        if (dotsToSend & this.SPACE) {
          if (dotsToSend !== this.SPACE) {
            // Can't combine space and actual dot keys.
            return true;
          }
          // Space is sent as a blank cell.
          dotsToSend = 0;
        }
        this.sendToChromeVox_({type: 'brailleDots', dots: dotsToSend});
      }
      return true;
    }
    return false;
  }

  /**
   * Connects to the ChromeVox extension for message passing.
   * @private
   */
  connectChromeVox_() {
    if (this.port_) {
      this.port_.disconnect();
      this.port_ = null;
    }
    this.port_ = chrome.runtime.connect(
        this.CHROMEVOX_EXTENSION_ID_, {name: this.PORT_NAME});
    this.port_.onMessage.addListener(this.onChromeVoxMessage_.bind(this));
    this.port_.onDisconnect.addListener(this.onChromeVoxDisconnect_.bind(this));
  }

  /**
   * Handles a message from the ChromeVox extension.
   * @param {*} message The message from the extension.
   * @private
   */
  onChromeVoxMessage_(message) {
    message = /** @type {{type: string}} */ (message);
    this.log_('onChromeVoxMessage', message);
    switch (message.type) {
      case 'replaceText':
        message =
            /**
             * @type {{contextID: number, deleteBefore: number,
             *         newText: string}}
             */
            (message);
        this.replaceText_(
            message.contextID, message.deleteBefore, message.newText);
        break;
      case 'keyEventHandled':
        message =
            /** @type {{requestId: string, result: boolean}} */ (message);
        this.keyEventHandled_(message.requestId, 'keydown', message.result);
        break;
      case 'setUncommitted':
        message =
            /** @type {{contextID: number, text: string}} */ (message);
        this.setUncommitted_(message.contextID, message.text);
        break;
      case 'commitUncommitted':
        message =
            /** @type {{contextID: number}} */ (message);
        this.commitUncommitted_(message.contextID);
        break;
      default:
        console.error(
            'Unknown message from ChromeVox: ' + JSON.stringify(message));
        break;
    }
  }

  /**
   * Handles a disconnect event from the ChromeVox side.
   * @private
   */
  onChromeVoxDisconnect_() {
    this.port_ = null;
    this.log_('onChromeVoxDisconnect', chrome.runtime.lastError);
  }

  /**
   * Sends a message to the ChromeVox extension.
   * @param {Object} message The message to send.
   * @private
   */
  sendToChromeVox_(message) {
    if (this.port_) {
      this.port_.postMessage(message);
    }
  }

  /**
   * Sends the given input context to ChromeVox.
   * @param {chrome.input.ime.InputContext} context Input context, or null
   *     when
   *    there's no input context.
   * @private
   */
  sendInputContext_(context) {
    this.sendToChromeVox_({type: 'inputContext', context});
  }

  /**
   * Sends the active state to ChromeVox.
   * @private
   */
  sendActiveState_() {
    this.sendToChromeVox_(
        {type: 'activeState', active: this.engineID_.length > 0});
  }

  /**
   * Replaces text in the current text field.
   * @param {number} contextID Context for the input field to replace the
   *     text in.
   * @param {number} deleteBefore How many characters to delete before the
   *     cursor.
   * @param {string} toInsert Text to insert at the cursor.
   */
  replaceText_(contextID, deleteBefore, toInsert) {
    var addText = chrome.input.ime.commitText.bind(
        null, {contextID, text: toInsert}, function() {});
    if (deleteBefore > 0) {
      var deleteText = chrome.input.ime.deleteSurroundingText.bind(
          null, {
            engineID: this.engineID_,
            contextID,
            offset: -deleteBefore,
            length: deleteBefore,
          },
          addText);
      // Make sure there's no non-zero length selection so that
      // deleteSurroundingText works correctly.
      chrome.input.ime.deleteSurroundingText(
          {engineID: this.engineID_, contextID, offset: 0, length: 0},
          deleteText);
    } else {
      addText();
    }
  }

  /**
   * Responds to an asynchronous key event, indicating whether it was handled
   * or not.  If it wasn't handled, any uncommitted text is committed
   * before sending the response to the IME API.
   * @param {string} requestId Key event request id.
   * @param {string} type Type of key event being responded to.
   * @param {boolean} response Whether the IME handled the event.
   */
  keyEventHandled_(requestId, type, response) {
    if (!response && type === 'keydown' && this.uncommitted_) {
      this.commitUncommitted_(this.uncommitted_.contextID);
      this.sendToChromeVox_({type: 'reset'});
    }
    chrome.input.ime.keyEventHandled(requestId, response);
  }

  /**
   * Stores uncommitted text that will be committed on any key press or
   * when {@code commitUncommitted_} is called.
   * @param {number} contextID of the current field.
   * @param {string} text to store.
   */
  setUncommitted_(contextID, text) {
    this.uncommitted_ = {contextID, text};
  }

  /**
   * Commits the last set uncommitted text if it matches the given context id.
   * @param {number} contextID
   */
  commitUncommitted_(contextID) {
    if (this.uncommitted_ && contextID === this.uncommitted_.contextID) {
      chrome.input.ime.commitText(this.uncommitted_);
    }
    this.uncommitted_ = null;
  }

  /**
   * Updates the menu items for this IME.
   */
  updateMenuItems_() {
    // TODO(plundblad): Localize when translations available.
    chrome.input.ime.setMenuItems({
      engineID: this.engineID_,
      items: [{
        id: this.USE_STANDARD_KEYBOARD_ID,
        label: 'Use standard keyboard for braille',
        style: 'check',
        visible: true,
        checked: this.useStandardKeyboard_,
        enabled: true,
      }],
    });
  }
}
