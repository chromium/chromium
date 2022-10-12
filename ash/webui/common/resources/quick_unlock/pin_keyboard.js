// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'pin-keyboard' is a keyboard that can be used to enter PINs or more generally
 * numeric values.
 *
 * Properties:
 *    value: The value of the PIN keyboard. Writing to this property will adjust
 *           the PIN keyboard's value.
 *
 * Events:
 *    pin-change: Fired when the PIN value has changed. The PIN is available at
 *                event.detail.pin.
 *    submit: Fired when the PIN is submitted. The PIN is available at
 *            event.detail.pin.
 *
 * Example:
 *    <pin-keyboard on-pin-change="onPinChange" on-submit="onPinSubmit">
 *    </pin-keyboard>
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './pin_keyboard_icons.html.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './pin_keyboard.html.js';

/**
 * Once auto backspace starts, the time between individual backspaces.
 * @type {number}
 * @const
 */
const REPEAT_BACKSPACE_DELAY_MS = 150;

/**
 * How long the backspace button must be held down before auto backspace
 * starts.
 * @type {number}
 * @const
 */
const INITIAL_BACKSPACE_DELAY_MS = 500;

/**
 * The key codes of the keys allowed to be used on the pin input, in addition to
 * number keys. We allow some editing keys. We also allow system keys, otherwise
 * preventDefault() will prevent the user from changing screen brightness,
 * taking screenshots, etc. https://crbug.com/1002863
 * @type {!Set<number>}
 * @const
 */
const PIN_INPUT_ALLOWED_NON_NUMBER_KEY_CODES = new Set([
  8,   // backspace
  9,   // tab
  37,  // left
  39,  // right
  // We don't allow back or forward.
  183,  // ZoomToggle, aka fullscreen
  182,  // LaunchApplication1, aka overview mode
  216,  // BrightnessDown
  217,  // BrightnessUp
  179,  // MediaPlayPause
  173,  // AudioVolumeMute
  174,  // AudioVolumeDown
  175,  // AudioVolumeUp
  154,  // LaunchControlPanel, aka system tray menu
]);

/**
 * @param {!Event} event
 * @return {boolean}
 */
function receivedEventFromKeyboard(event) {
  if (!event.detail || !event.detail.sourceEvent) {
    return false;
  }
  return event.detail.sourceEvent.detail === 0;
}

Polymer({
  _template: getTemplate(),
  is: 'pin-keyboard',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * Whether or not the keyboard's input element should be numerical
     * or password.
     */
    enablePassword: {
      type: Boolean,
      value: false,
    },

    // Whether or not non-digit pins are allowed.
    // If allowNonDigit is false, any characters typed in the pin dialog
    // will be swallowed.
    allowNonDigit: {
      type: Boolean,
      value: false,
    },

    hasError: Boolean,

    disabled: {
      type: Boolean,
      value: false,
    },

    /**
     * The password element the pin keyboard is associated with. If this is not
     * set, then a default input element is shown and used.
     * @type {?HTMLElement}
     */
    passwordElement: Object,

    /**
     * The intervalID used for the backspace button set/clear interval.
     * @private
     */
    repeatBackspaceIntervalId_: {
      type: Number,
      value: 0,
    },

    /**
     * The timeoutID used for the auto backspace.
     * @private
     */
    startAutoBackspaceId_: {
      type: Number,
      value: 0,
    },

    /**
     * The value stored in the keyboard's input element.
     */
    value: {
      type: String,
      notify: true,
      value: '',
      observer: 'onPinValueChange_',
    },

    /**
     * @private
     */
    focused_: {
      type: Boolean,
      value: false,
    },

    /**
     * Enables pin placeholder.
     */
    enablePlaceholder: {
      type: Boolean,
      value: false,
    },

    /**
     * The aria label to be used for the input element.
     */
    ariaLabel: {
      type: String,
    },
  },

  listeners: {
    'blur': 'onBlur_',
    'focus': 'onFocus_',
  },

  /**
   * Gets the selection start of the input field.
   * @type {number}
   * @private
   */
  get selectionStart_() {
    return this.passwordElement_().selectionStart;
  },

  /**
   * Gets the selection end of the input field.
   * @type {number}
   * @private
   */
  get selectionEnd_() {
    return this.passwordElement_().selectionEnd;
  },

  /**
   * Sets the selection start of the input field.
   * @param {number} start The new selection start of the input element.
   * @private
   */
  set selectionStart_(start) {
    this.passwordElement_().selectionStart = start;
  },

  /**
   * Sets the selection end of the input field.
   * @param {number} end The new selection end of the input element.
   * @private
   */
  set selectionEnd_(end) {
    this.passwordElement_().selectionEnd = end;
  },

  /**
   * Transfers blur to the input element.
   */
  blur() {
    this.passwordElement_().blur();
  },

  /**
   * Schedules a call to focusInputSynchronously().
   * @param {number=} opt_selectionStart
   * @param {number=} opt_selectionEnd
   */
  focusInput(opt_selectionStart, opt_selectionEnd) {
    setTimeout(
        () =>
            this.focusInputSynchronously(opt_selectionStart, opt_selectionEnd),
        0);
  },

  /**
   * Transfers focus to the input element. This should not bring up the virtual
   * keyboard, if it is enabled. After focus, moves the caret to the correct
   * location if specified.
   * @param {number=} opt_selectionStart
   * @param {number=} opt_selectionEnd
   */
  focusInputSynchronously(opt_selectionStart, opt_selectionEnd) {
    this.passwordElement_().focus();
    this.selectionStart_ = opt_selectionStart || 0;
    this.selectionEnd_ = opt_selectionEnd || 0;
  },

  /**
   * Transfers focus to the input. Called when a non button element on the
   * PIN button area is clicked to prevent focus from leaving the input.
   */
  onRootTap_() {
    // Focus the input and place the selected region to its exact previous
    // location, as this function will not be called by something that will also
    // modify the input value.
    this.focusInput(this.selectionStart_, this.selectionEnd_);
  },

  /** @private */
  onFocus_() {
    this.focused_ = true;
  },

  /** @private */
  onBlur_() {
    this.focused_ = false;
  },

  /**
   * Called when a keypad number has been tapped.
   * @param {!Event} event The event object.
   * @private
   */
  onNumberTap_(event) {
    const numberValue = event.target.getAttribute('value');

    // Add the number where the caret is, then update the selection range of the
    // input element.
    const selectionStart = this.selectionStart_;
    this.value = this.value.substring(0, this.selectionStart_) + numberValue +
        this.value.substring(this.selectionEnd_);

    // If a number button is clicked, we do not want to switch focus to the
    // button, therefore we transfer focus back to the input, but if a number
    // button is tabbed into, it should keep focus, so users can use tab and
    // spacebar/return to enter their PIN.
    if (!receivedEventFromKeyboard(event)) {
      this.focusInputSynchronously(selectionStart + 1, selectionStart + 1);
    }
    event.stopImmediatePropagation();
  },

  /** Fires a submit event with the current PIN value. */
  firePinSubmitEvent_() {
    this.fire('submit', {pin: this.value});
  },

  /**
   * Fires an update event with the current PIN value. The event will only be
   * fired if the PIN value has actually changed.
   * @param {string} value
   * @param {string} previous
   */
  onPinValueChange_(value, previous) {
    if (this.passwordElement) {
      this.passwordElement.value = value;
    }
    this.fire('pin-change', {pin: value});
  },

  /**
   * Called when the user wants to erase the last character of the entered
   * PIN value.
   * @private
   */
  onPinClear_() {
    // If the input is shown, clear the text based on the caret location or
    // selected region of the input element. If it is just a caret, remove the
    // character in front of the caret.
    let selectionStart = this.selectionStart_;
    const selectionEnd = this.selectionEnd_;
    if (selectionStart === selectionEnd && selectionStart) {
      selectionStart--;
    }

    this.value = this.value.substring(0, selectionStart) +
        this.value.substring(selectionEnd);

    // Move the caret or selected region to the correct new place.
    this.selectionStart_ = selectionStart;
    this.selectionEnd_ = selectionStart;
  },

  /**
   * Called when user taps the backspace the button. Only does something when
   * the tap comes from the keyboard. onBackspacePointerDown_ and
   * onBackspacePointerUp_ will handle the events if they come from mouse or
   * touch. Note: This does not support repeatedly backspacing by holding down
   * the space or enter key like touch or mouse does.
   * @param {!Event} event The event object.
   * @private
   */
  onBackspaceTap_(event) {
    if (!receivedEventFromKeyboard(event)) {
      return;
    }

    this.onPinClear_();
    this.clearAndReset_();
    event.stopImmediatePropagation();
  },

  /**
   * Called when the user presses or touches the backspace button. Starts a
   * timer which starts an interval to repeatedly backspace the pin value until
   * the interval is cleared.
   * @param {!Event} event The event object.
   * @private
   */
  onBackspacePointerDown_(event) {
    this.startAutoBackspaceId_ = setTimeout(function() {
      this.repeatBackspaceIntervalId_ =
          setInterval(this.onPinClear_.bind(this), REPEAT_BACKSPACE_DELAY_MS);
    }.bind(this), INITIAL_BACKSPACE_DELAY_MS);

    if (!receivedEventFromKeyboard(event)) {
      this.focusInput(this.selectionStart_, this.selectionEnd_);
    }
    event.stopImmediatePropagation();
  },

  /**
   * Helper function which clears the timer / interval ids and resets them.
   * @private
   */
  clearAndReset_() {
    clearInterval(this.repeatBackspaceIntervalId_);
    this.repeatBackspaceIntervalId_ = 0;
    clearTimeout(this.startAutoBackspaceId_);
    this.startAutoBackspaceId_ = 0;
  },

  /**
   * Called when the user unpresses or untouches the backspace button. Stops the
   * interval callback and fires a backspace event if there is no interval
   * running.
   * @param {!Event} event The event object.
   * @private
   */
  onBackspacePointerUp_(event) {
    // If an interval has started, do not fire event on pointer up.
    if (!this.repeatBackspaceIntervalId_) {
      this.onPinClear_();
    }
    this.clearAndReset_();

    // Since on-down gives the input element focus, the input element will
    // already have focus when on-up is called. This will actually bring up the
    // virtual keyboard, even if focusInput() is wrapped in a setTimeout. Blur
    // the input element first to workaround this.
    this.blur();
    if (!receivedEventFromKeyboard(event)) {
      this.focusInput(this.selectionStart_, this.selectionEnd_);
    }
    event.stopImmediatePropagation();
  },

  /**
   * Helper function to check whether a given |event| should be processed by
   * the input.
   * @param {Event} event The event object.
   * @private
   */
  isValidEventForInput_(event) {
    // Valid if the key is a non-digit and allowNonDigit is enabled.
    if (this.allowNonDigit) {
      return true;
    }

    // Valid if the key is a number, and shift is not pressed.
    if ((event.keyCode >= 48 && event.keyCode <= 57) && !event.shiftKey) {
      return true;
    }

    // Valid if the key is one of the selected special keys defined in
    // |PIN_INPUT_ALLOWED_NON_NUMBER_KEY_CODES|.
    if (PIN_INPUT_ALLOWED_NON_NUMBER_KEY_CODES.has(event.keyCode)) {
      return true;
    }

    // Valid if the key is CTRL+A to allow users to quickly select the entire
    // PIN.
    if (event.keyCode === 65 && event.ctrlKey) {
      return true;
    }

    // Valid if the key is CTRL+-, CTRL+=, or CTRL+0 to zoom in, zoom out, and
    // zoom reset the screen.
    if (event.ctrlKey && [48, 187, 189].includes(event.keyCode)) {
      return true;
    }

    // Valid if the key is Ctrl+Shift+Refresh to allow users rotate the screen
    if (event.keyCode === 168 && event.ctrlKey && event.shiftKey) {
      return true;
    }

    // Valid for the ChromeVox combination.
    if (event.ctrlKey && event.altKey && event.key === 'z') {
      return true;
    }

    // The rest of the keys are invalid.
    return false;
  },

  /**
   * Called when a key event is pressed while the input element has focus.
   * @param {Event} event The event object.
   * @private
   */
  onInputKeyDown_(event) {
    // Up/down pressed, swallow the event to prevent the input value from
    // being incremented or decremented.
    if (event.keyCode === 38 || event.keyCode === 40) {
      event.preventDefault();
      return;
    }

    // Enter pressed.
    if (event.keyCode === 13) {
      this.firePinSubmitEvent_();
      event.preventDefault();
      return;
    }

    // If only digits are allowed in the pin input (allowNonDigit is set to
    // false), then do not pass events that are not numbers or special keys we
    // care about. We use this instead of input type number because there are
    // several issues with input type number, such as no
    // selectionStart/selectionEnd and entered non numbers causes the caret to
    // jump to the left.
    if (!this.isValidEventForInput_(event)) {
      event.preventDefault();
      return;
    }
  },

  /**
   * Indicates if something is entered.
   * @private
   */
  hasInput_(value) {
    return value.length > 0;
  },

  /**
   * Determines if the pin input should be contrasted.
   * @private
   */
  hasInputOrFocus_(value, focused) {
    return this.hasInput_(value) || focused;
  },

  /**
   * Computes the value of the pin input placeholder.
   * @param {boolean} enablePassword
   * @param {boolean} enablePlaceholder
   * @private
   */
  getInputPlaceholder_(enablePassword, enablePlaceholder) {
    if (!enablePlaceholder) {
      return '';
    }

    return enablePassword ? this.i18n('pinKeyboardPlaceholderPinPassword') :
                            this.i18n('pinKeyboardPlaceholderPin');
  },

  /**
   * Computes the direction of the pin input.
   * @param {string} password
   * @private
   */
  isInputRtl_(password) {
    // +password will convert a string to a number or to NaN if that's not
    // possible. Number.isInteger will verify the value is not a NaN and that it
    // does not contain decimals.
    // This heuristic will fail for inputs like '1.0'.
    //
    // Since we still support users entering their passwords through the PIN
    // keyboard, we swap the input box to rtl when we think it is a password
    // (just numbers), if the document direction is rtl.
    return (document.dir === 'rtl') && !Number.isInteger(+password);
  },

  /**
   * @param {!MouseEvent} e
   * @private
   */
  onBackspaceContextMenu_(e) {
    // Note: If e.which is 0, this represents "no button" (i.e., a long-press).
    // If this event was triggered by another value (e.g., right click - 3),
    // return early and allow the context menu to be shown.
    if (e.which) {
      return;
    }

    // If the user was long-pressing the backspace button, that user likely was
    // trying to remove several numbers from the PIN text field rapidly, so
    // don't show the context menu.
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * @return {!HTMLElement} Returns the native input element of |pinInput|.
   * @private
   */
  passwordElement_() {
    // |passwordElement| is null by default. It can be set to override the
    // input field that will be populated with the keypad.
    return this.passwordElement ||
        (/** @type {CrInputElement} */ (this.$.pinInput)).inputElement;
  },
});
