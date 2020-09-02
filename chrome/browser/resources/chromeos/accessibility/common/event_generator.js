// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('EventGenerator');

goog.require('KeyCode');

/** Functions to send synthetic key and mouse events. */
EventGenerator = class {
  /**
   * Sends a single key stroke (down and up) with the given key code and
   *     keyboard modifiers (whether or not CTRL, ALT, SEARCH, and SHIFT are
   *     being held).
   * @param {!KeyCode} keyCode
   * @param {!chrome.accessibilityPrivate.SyntheticKeyboardModifiers} modifiers
   */
  static sendKeyPress(keyCode, modifiers = {}) {
    let type = chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN;
    chrome.accessibilityPrivate.sendSyntheticKeyEvent(
        {type, keyCode, modifiers});

    type = chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP;
    chrome.accessibilityPrivate.sendSyntheticKeyEvent(
        {type, keyCode, modifiers});
  }

  /**
   * Sends two synthetic mouse events (a mouse press and and a mouse release)
   *     to simulate a mouse click.
   * @param {number} x
   * @param {number} y
   * @param {number} delayMs The delay between mouse press and mouse release,
   *     in milliseconds.
   */
  static sendMouseClick(x, y, delayMs = 0) {
    if (EventGenerator.currentlyMidMouseClick) {
      EventGenerator.mouseClickQueue.push(arguments);
      return;
    }
    EventGenerator.currentlyMidMouseClick = true;

    let type = chrome.accessibilityPrivate.SyntheticMouseEventType.PRESS;
    chrome.accessibilityPrivate.sendSyntheticMouseEvent({type, x, y});

    const callback = () => {
      type = chrome.accessibilityPrivate.SyntheticMouseEventType.RELEASE;
      chrome.accessibilityPrivate.sendSyntheticMouseEvent({type, x, y});

      EventGenerator.currentlyMidMouseClick = false;
      if (EventGenerator.mouseClickQueue.length > 0) {
        EventGenerator.sendMouseClick.apply(
            null /* this */, EventGenerator.mouseClickQueue.shift());
      }
    };
    if (delayMs > 0) {
      setTimeout(callback, delayMs);
    } else {
      callback();
    }
  }

  /**
   * Sends a synthetic mouse event to simulate a move event.
   * @param {number} x
   * @param {number} y
   * @param {boolean} touchAccessibility
   */
  static sendMouseMove(x, y, touchAccessibility = false) {
    const type = chrome.accessibilityPrivate.SyntheticMouseEventType.MOVE;
    chrome.accessibilityPrivate.sendSyntheticMouseEvent(
        {type, x, y, touchAccessibility});
  }
};

EventGenerator.mouseClickQueue = [];
