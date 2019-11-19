// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let SelectToSpeakState = chrome.accessibilityPrivate.SelectToSpeakState;

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
 * @param {SelectToSpeakCallbacks} callbacks
 * @constructor
 */
let InputHandler = function(callbacks) {
  /** @private {SelectToSpeakCallbacks} */
  this.callbacks_ = callbacks;

  /** @private {boolean} */
  this.trackingMouse_ = false;

  /** @private {boolean} */
  this.didTrackMouse_ = false;

  /** @private {boolean} */
  this.isSearchKeyDown_ = false;

  /** @private {boolean} */
  this.isSelectionKeyDown_ = false;

  /** @private {!Set<number>} */
  this.keysCurrentlyDown_ = new Set();

  /** @private {!Set<number>} */
  this.keysPressedTogether_ = new Set();

  /** @private {{x: number, y: number}} */
  this.mouseStart_ = {x: 0, y: 0};

  /** @private {{x: number, y: number}} */
  this.mouseEnd_ = {x: 0, y: 0};

  /**
   * The timestamp at which clipboard data read was requested by the user
   * doing a "read selection" keystroke on a Google Docs app. If a
   * clipboard change event comes in within CLIPBOARD_READ_MAX_DELAY_MS,
   * Select-to-Speak will read that text out loud.
   * @private {Date}
   */
  this.lastReadClipboardDataTime_ = new Date(0);

  /**
   * The timestamp at which the last clipboard data clear was requested.
   * Used to make sure we don't clear the clipboard on a user's request,
   * but only after the clipboard was used to read selected text.
   * @private {Date}
   */
  this.lastClearClipboardDataTime_ = new Date(0);

  /**
   * Called when the mouse is moved or dragged and the user is in a
   * mode where select-to-speak is capturing mouse events (for example
   * holding down Search).
   *
   * @param {!Event} evt The DOM event
   * @return {boolean} True if the default action should be performed.
   * @private
   */
  this.onMouseMove_ = function(evt) {
    if (!this.trackingMouse_) {
      return false;
    }

    var rect = RectUtils.rectFromPoints(
        this.mouseStart_.x, this.mouseStart_.y, evt.screenX, evt.screenY);
    this.callbacks_.onSelectionChanged(rect);
    return false;
  };

  /**
   * @private
   */
  this.onClipboardDataChanged_ = function() {
    if (new Date() - this.lastReadClipboardDataTime_ <
        InputHandler.CLIPBOARD_READ_MAX_DELAY_MS) {
      // The data has changed, and we are ready to read it.
      // Get it using a paste.
      document.execCommand('paste');
    }
  };

  /**
   * @private
   */
  this.onClipboardCopy_ = function(evt) {
    if (new Date() - this.lastClearClipboardDataTime_ <
        InputHandler.CLIPBOARD_CLEAR_MAX_DELAY_MS) {
      // onClipboardPaste has just completed reading the clipboard for speech.
      // This is used to clear the clipboard.
      evt.clipboardData.setData('text/plain', '');
      evt.preventDefault();
      this.lastClearClipboardDataTime_ = new Date(0);
    }
  };

  /**
   * @private
   */
  this.onClipboardPaste_ = function(evt) {
    if (new Date() - this.lastReadClipboardDataTime_ <
        InputHandler.CLIPBOARD_READ_MAX_DELAY_MS) {
      // Read the current clipboard data.
      evt.preventDefault();
      this.callbacks_.onTextReceived(evt.clipboardData.getData('text/plain'));
      this.lastReadClipboardDataTime_ = new Date(0);
      // Clear the clipboard data by copying nothing (the current document).
      // Do this in a timeout to avoid a recursive warning per
      // https://crbug.com/363288.
      setTimeout(() => {
        this.lastClearClipboardDataTime_ = new Date();
        document.execCommand('copy');
      }, 0);
    }
  };
};

InputHandler.prototype = {
  /**
   * Set up event listeners for mouse and keyboard events. These are
   * forwarded to us from the SelectToSpeakEventHandler so they should
   * be interpreted as global events on the whole screen, not local to
   * any particular window.
   * @public
   */
  setUpEventListeners: function() {
    document.addEventListener('keydown', this.onKeyDown_.bind(this));
    document.addEventListener('keyup', this.onKeyUp_.bind(this));
    document.addEventListener('mousedown', this.onMouseDown_.bind(this));
    document.addEventListener('mousemove', this.onMouseMove_.bind(this));
    document.addEventListener('mouseup', this.onMouseUp_.bind(this));
    chrome.clipboard.onClipboardDataChanged.addListener(
        this.onClipboardDataChanged_.bind(this));
    document.addEventListener('paste', this.onClipboardPaste_.bind(this));
    document.addEventListener('copy', this.onClipboardCopy_.bind(this));
  },

  /**
   * Change whether or not we are tracking the mouse.
   * @param {boolean} tracking True if we should start tracking the mouse, false
   *     otherwise.
   * @public
   */
  setTrackingMouse: function(tracking) {
    this.trackingMouse_ = tracking;
  },

  /**
   * Gets the rect that has been drawn by clicking and dragging the mouse.
   * @public
   */
  getMouseRect: function() {
    return RectUtils.rectFromPoints(
        this.mouseStart_.x, this.mouseStart_.y, this.mouseEnd_.x,
        this.mouseEnd_.y);
  },

  /**
   * Sets the date at which we last wanted the clipboard data to be read.
   * @public
   */
  onRequestReadClipboardData: function() {
    this.lastReadClipboardDataTime_ = new Date();
  },

  /**
   * Called when the mouse is pressed and the user is in a mode where
   * select-to-speak is capturing mouse events (for example holding down
   * Search).
   * Visible for testing.
   *
   * @param {!Event} evt The DOM event
   * @return {boolean} True if the default action should be performed;
   *    we always return false because we don't want any other event
   *    handlers to run.
   * @public
   */
  onMouseDown_: function(evt) {
    // If the user hasn't clicked 'search', or if they are currently
    // trying to highlight a selection, don't track the mouse.
    if (this.callbacks_.canStartSelecting() &&
        (!this.isSearchKeyDown_ || this.isSelectionKeyDown_))
      return false;

    this.callbacks_.onSelectingStateChanged(
        true /* is selecting */, evt.screenX, evt.screenY);

    this.trackingMouse_ = true;
    this.didTrackMouse_ = true;
    this.mouseStart_ = {x: evt.screenX, y: evt.screenY};
    this.onMouseMove_(evt);

    return false;
  },

  /**
   * Called when the mouse is released and the user is in a
   * mode where select-to-speak is capturing mouse events (for example
   * holding down Search).
   * Visible for testing.
   *
   * @param {!Event} evt
   * @return {boolean} True if the default action should be performed.
   * @public
   */
  onMouseUp_: function(evt) {
    if (!this.trackingMouse_) {
      return false;
    }
    this.onMouseMove_(evt);
    this.trackingMouse_ = false;
    if (!this.keysCurrentlyDown_.has(SelectToSpeak.SEARCH_KEY_CODE)) {
      // This is only needed to cancel something started with the search key.
      this.didTrackMouse_ = false;
    }


    this.mouseEnd_ = {x: evt.screenX, y: evt.screenY};
    var ctrX = Math.floor((this.mouseStart_.x + this.mouseEnd_.x) / 2);
    var ctrY = Math.floor((this.mouseStart_.y + this.mouseEnd_.y) / 2);

    this.callbacks_.onSelectingStateChanged(
        false /* is no longer selecting */, ctrX, ctrY);

    return false;
  },

  /**
   * Visible for testing.
   * @param {!Event} evt
   * @public
   */
  onKeyDown_: function(evt) {
    this.keysCurrentlyDown_.add(evt.keyCode);
    this.keysPressedTogether_.add(evt.keyCode);
    if (this.keysPressedTogether_.size == 1 &&
        evt.keyCode == SelectToSpeak.SEARCH_KEY_CODE) {
      this.isSearchKeyDown_ = true;
    } else if (
        this.keysCurrentlyDown_.size == 2 &&
        evt.keyCode == SelectToSpeak.READ_SELECTION_KEY_CODE &&
        !this.trackingMouse_) {
      // Only go into selection mode if we aren't already tracking the mouse.
      this.isSelectionKeyDown_ = true;
    } else if (!this.trackingMouse_) {
      // Some other key was pressed.
      this.isSearchKeyDown_ = false;
    }
  },

  /**
   * Visible for testing.
   * @param {!Event} evt
   * @public
   */
  onKeyUp_: function(evt) {
    if (evt.keyCode == SelectToSpeak.READ_SELECTION_KEY_CODE) {
      if (this.isSelectionKeyDown_ && this.keysPressedTogether_.size == 2 &&
          this.keysPressedTogether_.has(evt.keyCode) &&
          this.keysPressedTogether_.has(SelectToSpeak.SEARCH_KEY_CODE)) {
        this.callbacks_.onKeystrokeSelection();
      }
      this.isSelectionKeyDown_ = false;
    } else if (evt.keyCode == SelectToSpeak.SEARCH_KEY_CODE) {
      this.isSearchKeyDown_ = false;

      // If we were in the middle of tracking the mouse, cancel it.
      if (this.trackingMouse_) {
        this.trackingMouse_ = false;
        this.callbacks_.onRequestCancel();
      }
    }

    // Stop speech when the user taps and releases Control or Search
    // without using the mouse or pressing any other keys along the way.
    if (!this.didTrackMouse_ &&
        (evt.keyCode == SelectToSpeak.SEARCH_KEY_CODE ||
         evt.keyCode == SelectToSpeak.CONTROL_KEY_CODE) &&
        this.keysPressedTogether_.has(evt.keyCode) &&
        this.keysPressedTogether_.size == 1) {
      this.trackingMouse_ = false;
      this.callbacks_.onRequestCancel();
    }

    this.keysCurrentlyDown_.delete(evt.keyCode);
    if (this.keysCurrentlyDown_.size == 0) {
      this.keysPressedTogether_.clear();
      this.didTrackMouse_ = false;
    }
  },
};

// Number of milliseconds to wait after requesting a clipboard read
// before clipboard change and paste events are ignored.
InputHandler.CLIPBOARD_READ_MAX_DELAY_MS = 1000;

// Number of milliseconds to wait after requesting a clipboard copy
// before clipboard copy events are ignored, used to clear the clipboard
// after reading data in a paste event.
InputHandler.CLIPBOARD_CLEAR_MAX_DELAY_MS = 500;
