// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RectUtil} from '../common/rect_util.js';

import {SelectToSpeakConstants} from './select_to_speak_constants.js';

const SelectToSpeakState = chrome.accessibilityPrivate.SelectToSpeakState;
const SyntheticMouseEventType =
    chrome.accessibilityPrivate.SyntheticMouseEventType;

/**
 * Callbacks for InputHandler.
 * |canStartSelecting| returns true if the user can start selecting a region
 * with the mouse. |onSelectingStateChanged| is called when the user starts or
 * ends selecting with the mouse. |onSelectionChanged| is called when the
 * region selected with the mouse changes size. |onKeystrokeSelection| is called
 * when a keystroke is completed indicating that highlighted text is selected to
 * be used. |onRequestCancel| is called when the user has indicated that the
 * current selection or speech should be canceled. |onTextReceived| is called
 * when a copy-paste event results in text to be spoken.
 * @typedef {{
 *     canStartSelecting: function(): boolean,
 *     onSelectingStateChanged: function(boolean, number, number),
 *     onSelectionChanged: function({left: number, top: number, width: number,
 *                                   height: number}),
 *     onKeystrokeSelection: function(),
 *     onRequestCancel: function(),
 *     onTextReceived: function(string)
 * }}
 */
let SelectToSpeakCallbacks;

/**
 * Class to handle user-input, from mouse, keyboard, and copy-paste events.
 */
export class InputHandler {
  /**
   * Please keep fields in alphabetical order.
   * @param {!SelectToSpeakCallbacks} callbacks
   */
  constructor(callbacks) {
    /** @private {!SelectToSpeakCallbacks} */
    this.callbacks_ = callbacks;

    /** @private {boolean} */
    this.didTrackMouse_ = false;

    /** @private {boolean} */
    this.isSearchKeyDown_ = false;

    /** @private {boolean} */
    this.isSelectionKeyDown_ = false;

    /** @private {!Set<number>} */
    this.keysCurrentlyDown_ = new Set();

    /**
     * All of the keys pressed since the last time 0 keys were pressed.
     * @private {!Set<number>}
     */
    this.keysPressedTogether_ = new Set();

    /**
     * The timestamp at which the last clipboard data clear was requested.
     * Used to make sure we don't clear the clipboard on a user's request,
     * but only after the clipboard was used to read selected text.
     * @private {Date}
     */
    this.lastClearClipboardDataTime_ = new Date(0);

    /**
     * The timestamp at which clipboard data read was requested by the user
     * doing a "read selection" keystroke on a Google Docs app. If a
     * clipboard change event comes in within CLIPBOARD_READ_MAX_DELAY_MS,
     * Select-to-Speak will read that text out loud.
     * @private {Date}
     */
    this.lastReadClipboardDataTime_ = new Date(0);

    /** @private {{x: number, y: number}} */
    this.mouseStart_ = {x: 0, y: 0};

    /** @private {{x: number, y: number}} */
    this.mouseEnd_ = {x: 0, y: 0};

    /** @private {boolean} */
    this.trackingMouse_ = false;
  }

  /** @private */
  clearClipboard_() {
    this.lastClearClipboardDataTime_ = new Date();
    document.execCommand('copy');
  }

  /**
   * @param {Event} evt
   * @private
   */
  onClipboardCopy_(evt) {
    if (new Date() - this.lastClearClipboardDataTime_ <
        InputHandler.CLIPBOARD_CLEAR_MAX_DELAY_MS) {
      // onClipboardPaste has just completed reading the clipboard for speech.
      // This is used to clear the clipboard.
      evt.clipboardData.setData('text/plain', '');
      evt.preventDefault();
      this.lastClearClipboardDataTime_ = new Date(0);
    }
  }

  /** @private */
  onClipboardDataChanged_() {
    if (new Date() - this.lastReadClipboardDataTime_ <
        InputHandler.CLIPBOARD_READ_MAX_DELAY_MS) {
      // The data has changed, and we are ready to read it.
      // Get it using a paste.
      document.execCommand('paste');
    }
  }

  /**
   * @param {Event} evt
   * @private
   */
  onClipboardPaste_(evt) {
    if (new Date() - this.lastReadClipboardDataTime_ <
        InputHandler.CLIPBOARD_READ_MAX_DELAY_MS) {
      // Read the current clipboard data.
      evt.preventDefault();
      this.callbacks_.onTextReceived(evt.clipboardData.getData('text/plain'));
      this.lastReadClipboardDataTime_ = new Date(0);
      // Clear the clipboard data by copying nothing (the current document).
      // Do this in a timeout to avoid a recursive warning per
      // https://crbug.com/363288.
      setTimeout(() => this.clearClipboard_(), 0);
    }
  }

  /**
   * Set up event listeners for mouse and keyboard events. These are
   * forwarded to us from the SelectToSpeakEventHandler so they should
   * be interpreted as global events on the whole screen, not local to
   * any particular window.
   */
  setUpEventListeners() {
    chrome.clipboard.onClipboardDataChanged.addListener(
        () => this.onClipboardDataChanged_());
    document.addEventListener('paste', evt => this.onClipboardPaste_(evt));
    document.addEventListener('copy', evt => this.onClipboardCopy_(evt));
    chrome.accessibilityPrivate.onSelectToSpeakKeysPressedChanged.addListener(
        (keysPressed) => {
          this.onKeysPressedChanged_(new Set(keysPressed));
        });
    chrome.accessibilityPrivate.onSelectToSpeakMouseChanged.addListener(
        (eventType, mouseX, mouseY) => {
          this.onMouseEvent_(eventType, mouseX, mouseY);
        });
  }

  /**
   * Change whether or not we are tracking the mouse.
   * @param {boolean} tracking True if we should start tracking the mouse, false
   *     otherwise.
   */
  setTrackingMouse(tracking) {
    this.trackingMouse_ = tracking;
  }

  /**
   * Gets the rect that has been drawn by clicking and dragging the mouse.
   */
  getMouseRect() {
    return RectUtil.rectFromPoints(
        this.mouseStart_.x, this.mouseStart_.y, this.mouseEnd_.x,
        this.mouseEnd_.y);
  }

  /**
   * Sets the date at which we last wanted the clipboard data to be read.
   */
  onRequestReadClipboardData() {
    this.lastReadClipboardDataTime_ = new Date();
  }

  /**
   * Called when the mouse is pressed, released or moved and the user is
   * in a mode where select-to-speak is capturing mouse events (for example
   * holding down Search).
   * Visible for testing.
   *
   * @param {!SyntheticMouseEventType} type The event type.
   * @param {number} mouseX The mouse x coordinate in global screen
   *     coordinates.
   * @param {number} mouseY The mouse y coordinate in global screen
   *     coordinates.
   */
  onMouseEvent_(type, mouseX, mouseY) {
    if (type === SyntheticMouseEventType.PRESS) {
      this.onMouseDown_(mouseX, mouseY);
    } else if (type === SyntheticMouseEventType.RELEASE) {
      this.onMouseUp_(mouseX, mouseY);
    } else {
      this.onMouseMove_(mouseX, mouseY);
    }
  }

  /**
   * Called when the mouse is pressed and the user is in a
   * mode where select-to-speak is capturing mouse events (for example
   * holding down Search).
   * @param {number} mouseX The mouse x coordinate in global screen
   *     coordinates.
   * @param {number} mouseY The mouse y coordinate in global screen
   *     coordinates.
   * @private
   */
  onMouseDown_(mouseX, mouseY) {
    // If the user hasn't clicked 'search', or if they are currently
    // trying to highlight a selection, don't track the mouse.
    if (this.callbacks_.canStartSelecting() &&
        (!this.isSearchKeyDown_ || this.isSelectionKeyDown_)) {
      return false;
    }

    this.callbacks_.onSelectingStateChanged(
        true /* is selecting */, mouseX, mouseY);

    this.trackingMouse_ = true;
    this.didTrackMouse_ = true;
    this.mouseStart_ = {x: mouseX, y: mouseY};
    this.onMouseMove_(mouseX, mouseY);

    return false;
  }

  /**
   * Called when the mouse is moved or dragged and the user is in a
   * mode where select-to-speak is capturing mouse events (for example
   * holding down Search).
   * @param {number} mouseX The mouse x coordinate in global screen
   *     coordinates.
   * @param {number} mouseY The mouse y coordinate in global screen
   *     coordinates.
   * @private
   */
  onMouseMove_(mouseX, mouseY) {
    if (!this.trackingMouse_) {
      return;
    }

    const rect = RectUtil.rectFromPoints(
        this.mouseStart_.x, this.mouseStart_.y, mouseX, mouseY);
    this.callbacks_.onSelectionChanged(rect);
  }

  /**
   * Called when the mouse is released and the user is in a
   * mode where select-to-speak is capturing mouse events (for example
   * holding down Search).
   * @param {number} mouseX The mouse x coordinate in global screen
   *     coordinates.
   * @param {number} mouseY The mouse y coordinate in global screen
   *     coordinates.
   * @private
   */
  onMouseUp_(mouseX, mouseY) {
    if (!this.trackingMouse_) {
      return;
    }
    this.onMouseMove_(mouseX, mouseY);
    this.trackingMouse_ = false;
    if (!this.keysCurrentlyDown_.has(SelectToSpeakConstants.SEARCH_KEY_CODE)) {
      // This is only needed to cancel something started with the search key.
      this.didTrackMouse_ = false;
    }

    this.mouseEnd_ = {x: mouseX, y: mouseY};
    var ctrX = Math.floor((this.mouseStart_.x + this.mouseEnd_.x) / 2);
    var ctrY = Math.floor((this.mouseStart_.y + this.mouseEnd_.y) / 2);

    this.callbacks_.onSelectingStateChanged(
        false /* is no longer selecting */, ctrX, ctrY);
  }

  /**
   * Visible for testing.
   * @param {!Set<number>} keysCurrentlyPressed
   */
  onKeysPressedChanged_(keysCurrentlyPressed) {
    if (keysCurrentlyPressed.size > this.keysCurrentlyDown_.size) {
      // If a key was pressed.
      for (const key of keysCurrentlyPressed) {
        // Union with keysPressedTogether_ to track all the keys that have been
        // pressed.
        this.keysPressedTogether_.add(key);
      }
      if (this.keysPressedTogether_.size === 1 &&
          keysCurrentlyPressed.has(SelectToSpeakConstants.SEARCH_KEY_CODE)) {
        this.isSearchKeyDown_ = true;
      } else if (
          this.isSearchKeyDown_ && keysCurrentlyPressed.size === 2 &&
          keysCurrentlyPressed.has(
              SelectToSpeakConstants.READ_SELECTION_KEY_CODE) &&
          !this.trackingMouse_) {
        // Only go into selection mode if we aren't already tracking the mouse.
        this.isSelectionKeyDown_ = true;
      } else if (!this.trackingMouse_) {
        // Some other key was pressed.
        this.isSearchKeyDown_ = false;
      }
    } else {
      // If a key was released.
      const searchKeyReleased =
          this.keysCurrentlyDown_.has(SelectToSpeakConstants.SEARCH_KEY_CODE) &&
          !keysCurrentlyPressed.has(SelectToSpeakConstants.SEARCH_KEY_CODE);
      const ctrlKeyReleased = this.keysCurrentlyDown_.has(
                                  SelectToSpeakConstants.CONTROL_KEY_CODE) &&
          !keysCurrentlyPressed.has(SelectToSpeakConstants.CONTROL_KEY_CODE);
      const speakSelectionKeyReleased =
          this.keysCurrentlyDown_.has(
              SelectToSpeakConstants.READ_SELECTION_KEY_CODE) &&
          !keysCurrentlyPressed.has(
              SelectToSpeakConstants.READ_SELECTION_KEY_CODE);
      if (speakSelectionKeyReleased) {
        if (this.isSelectionKeyDown_ && this.keysPressedTogether_.size === 2 &&
            this.keysPressedTogether_.has(
                SelectToSpeakConstants.SEARCH_KEY_CODE)) {
          this.callbacks_.onKeystrokeSelection();
        }
        this.isSelectionKeyDown_ = false;
      } else if (searchKeyReleased) {
        // Search key released.
        this.isSearchKeyDown_ = false;
        // If we were in the middle of tracking the mouse, cancel it.
        if (this.trackingMouse_) {
          this.trackingMouse_ = false;
          this.callbacks_.onRequestCancel();
        }
      }
      // Stop speech when the user taps and releases Control or Search
      // without using the mouse or pressing any other keys along the way.
      if (!this.didTrackMouse_ && (ctrlKeyReleased || searchKeyReleased) &&
          this.keysPressedTogether_.size === 1) {
        this.trackingMouse_ = false;
        this.callbacks_.onRequestCancel();
      }
      // We don't remove from keysPressedTogether_ because it tracks all the
      // keys which were pressed since pressing started.
    }

    // Reset our state with the Chrome OS key state. This ensures that even if
    // we miss a key event (may happen during login/logout/screensaver?) we
    // quickly get back to the correct state.
    this.keysCurrentlyDown_ = keysCurrentlyPressed;
    if (this.keysCurrentlyDown_.size === 0) {
      this.didTrackMouse_ = false;
      this.keysPressedTogether_.clear();
    }
  }
}

// Number of milliseconds to wait after requesting a clipboard read
// before clipboard change and paste events are ignored.
InputHandler.CLIPBOARD_READ_MAX_DELAY_MS = 1000;

// Number of milliseconds to wait after requesting a clipboard copy
// before clipboard copy events are ignored, used to clear the clipboard
// after reading data in a paste event.
InputHandler.CLIPBOARD_CLEAR_MAX_DELAY_MS = 500;
