// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Key} from './key_code.js';

export interface MouseClickParams {
  delayMs?: number;
  mouseButton?: chrome.accessibilityPrivate.SyntheticMouseEventButton;
}

interface MouseClick {
  x: number;
  y: number;
  params: MouseClickParams;
}

/** Functions to send synthetic key and mouse events. */
export class EventGenerator {
  static currentlyMidMouseClick = false;
  static mouseClickQueue: MouseClick[] = [];

  /**
   * Sends a single key stroke (down and up) with the given key code and
   *     keyboard modifiers (whether or not CTRL, ALT, SEARCH, and SHIFT are
   *     being held).
   * @param useRewriters If true, uses rewriters for the key event;
   *     only allowed if used from Dictation. Otherwise indicates that rewriters
   *     should be skipped.
   */
  static sendKeyPress(
      keyCode: Key.Code,
      modifiers: chrome.accessibilityPrivate.SyntheticKeyboardModifiers = {},
      useRewriters = false): void {
    let type = chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN;
    chrome.accessibilityPrivate.sendSyntheticKeyEvent(
        {type, keyCode, modifiers}, useRewriters);

    type = chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP;
    chrome.accessibilityPrivate.sendSyntheticKeyEvent(
        {type, keyCode, modifiers}, useRewriters);
  }

  /**
   * Sends two synthetic mouse events (a mouse press and and a mouse release)
   *     to simulate a mouse click.
   */
  static sendMouseClick(x: number, y: number, params: MouseClickParams = {
    delayMs: 0,
    mouseButton: chrome.accessibilityPrivate.SyntheticMouseEventButton.LEFT,
  }): void {
    // TODO(b/314203187): Set params.delayMs ??= 0; and params.mouseButton =
    // chrome.accessibilityPrivate.SyntheticMouseEventButton.LEFT;. This fixes
    // the case when a params object is passed in, but only one of the params is
    // specified. In that case, the other default property isn't included in the
    // object (since the whole default params object gets shadowed).
    if (EventGenerator.currentlyMidMouseClick) {
      EventGenerator.mouseClickQueue.push({x, y, params});
      return;
    }
    EventGenerator.currentlyMidMouseClick = true;

    // chrome.accessibilityPrivate.sendSyntheticMouseEvent only accepts
    // integers.
    x = Math.round(x);
    y = Math.round(y);

    const {delayMs, mouseButton} = params;
    let type = chrome.accessibilityPrivate.SyntheticMouseEventType.PRESS;
    chrome.accessibilityPrivate.sendSyntheticMouseEvent(
        {type, x, y, mouseButton});

    const callback = (): void => {
      type = chrome.accessibilityPrivate.SyntheticMouseEventType.RELEASE;
      chrome.accessibilityPrivate.sendSyntheticMouseEvent(
          {type, x, y, mouseButton});

      EventGenerator.currentlyMidMouseClick = false;
      if (EventGenerator.mouseClickQueue.length > 0) {
        const {x, y, params} = EventGenerator.mouseClickQueue.shift()!;
        EventGenerator.sendMouseClick(x, y, params);
      }
    };
    // TODO(b/314203187): Not null asserted, check these to make sure this
    // is correct.
    if (delayMs! > 0) {
      setTimeout(callback, delayMs);
    } else {
      callback();
    }
  }

  /** Sends a synthetic mouse event to simulate a move event. */
  static sendMouseMove(x: number, y: number, touchAccessibility = false): void {
    const type = chrome.accessibilityPrivate.SyntheticMouseEventType.MOVE;
    chrome.accessibilityPrivate.sendSyntheticMouseEvent(
        {type, x, y, touchAccessibility});
  }
}
