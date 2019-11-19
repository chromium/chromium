// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Functions to send synthetic key and mouse events. */
const EventHelper = {
  /**
   * Sends a single key stroke (down and up) with the given key code and
   *     keyboard modifiers (whether or not CTRL, ALT, SEARCH, and SHIFT are
   *     being held).
   * @param {!EventHelper.KeyCode} keyCode
   * @param {!chrome.accessibilityPrivate.SyntheticKeyboardModifiers} modifiers
   */
  simulateKeyPress: (keyCode, modifiers = {}) => {
    let type = chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN;
    chrome.accessibilityPrivate.sendSyntheticKeyEvent(
        {type, keyCode, modifiers});

    type = chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP;
    chrome.accessibilityPrivate.sendSyntheticKeyEvent(
        {type, keyCode, modifiers});
  },

  /**
   * Sends a synthetic mouse event.
   * @param {number} x
   * @param {number} y
   * @param {number=} delayMs The delay between mouse press and mouse release,
   *     in milliseconds.
   */
  simulateMouseClick: (x, y, delayMs) => {
    let type = chrome.accessibilityPrivate.SyntheticMouseEventType.PRESS;
    chrome.accessibilityPrivate.sendSyntheticMouseEvent({type, x, y});

    let callback = () => {
      type = chrome.accessibilityPrivate.SyntheticMouseEventType.RELEASE;
      chrome.accessibilityPrivate.sendSyntheticMouseEvent({type, x, y});
    };

    if (delayMs) {
      setTimeout(callback, delayMs);
    } else {
      callback();
    }
  },

  /**
   * Defines the key codes for specified keys.
   * @enum {number}
   * @const
   */
  KeyCode: {
    ESC: 27,
    END: 35,
    HOME: 36,
    LEFT_ARROW: 37,
    UP_ARROW: 38,
    RIGHT_ARROW: 39,
    DOWN_ARROW: 40,
    C: 67,
    V: 86,
    X: 88
  }
};
