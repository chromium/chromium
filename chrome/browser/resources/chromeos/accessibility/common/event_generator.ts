// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KeyCode} from './key_code.js';
import {TestImportManager} from './testing/test_import_manager.js';

export interface MouseClickParams {
  // The amount of time to wait between press and release.
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
  static midMouseClickButton:
      chrome.accessibilityPrivate.SyntheticMouseEventButton|undefined;
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
      keyCode: KeyCode,
      modifiers: chrome.accessibilityPrivate.SyntheticKeyboardModifiers = {},
      useRewriters = false): void {
    EventGenerator.sendKeyDown(keyCode, modifiers, useRewriters);
    EventGenerator.sendKeyUp(keyCode, modifiers, useRewriters);
  }

  /**
   * Sends a single key down event with the given key code and
   *     keyboard modifiers (whether or not CTRL, ALT, SEARCH, and SHIFT are
   *     being held).
   * @param useRewriters If true, uses rewriters for the key event;
   *     only allowed if used from Dictation. Otherwise indicates that rewriters
   *     should be skipped.
   */
  static sendKeyDown(
      keyCode: KeyCode,
      modifiers: chrome.accessibilityPrivate.SyntheticKeyboardModifiers = {},
      useRewriters = false): void {
    const type = chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN;
    chrome.accessibilityPrivate.sendSyntheticKeyEvent(
        {type, keyCode, modifiers}, useRewriters);
  }

  /**
   * Sends a single key up event with the given key code and
   *     keyboard modifiers (whether or not CTRL, ALT, SEARCH, and SHIFT are
   *     being held).
   * @param useRewriters If true, uses rewriters for the key event;
   *     only allowed if used from Dictation. Otherwise indicates that rewriters
   *     should be skipped.
   */
  static sendKeyUp(
      keyCode: KeyCode,
      modifiers: chrome.accessibilityPrivate.SyntheticKeyboardModifiers = {},
      useRewriters = false): void {
    const type = chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP;
    chrome.accessibilityPrivate.sendSyntheticKeyEvent(
        {type, keyCode, modifiers}, useRewriters);
  }

  /**
   * Sends a synthetic mouse press (a mouse press and and a mouse release)
   *     to simulate a mouse click.
   */
  static sendMouseClick(x: number, y: number, params: MouseClickParams = {
    delayMs: 0,
    mouseButton: chrome.accessibilityPrivate.SyntheticMouseEventButton.LEFT,
  }): void {
    const delayMs = params.delayMs ? params.delayMs : 0;
    const mouseButton = params.mouseButton ?
        params.mouseButton :
        chrome.accessibilityPrivate.SyntheticMouseEventButton.LEFT;

    if (EventGenerator.midMouseClickButton !== undefined) {
      // Add it to the queue for later.
      EventGenerator.mouseClickQueue.push({x, y, params});
      return;
    }

    EventGenerator.sendMousePress(x, y, mouseButton);

    if (delayMs > 0) {
      setTimeout(() => EventGenerator.sendMouseRelease(x, y), params.delayMs);
    } else {
      EventGenerator.sendMouseRelease(x, y);
    }
  }

  /**
   * Sends a synthetic mouse press event, if we are not in the middle of a
   * mouse click event. If we are in the middle of a mouse click, returns
   * false as no press event was sent.
   */
  static sendMousePress(
      x: number, y: number,
      mouseButton: chrome.accessibilityPrivate.SyntheticMouseEventButton,
      isDoubleClick = false): boolean {
    if (EventGenerator.midMouseClickButton !== undefined) {
      return false;
    }

    EventGenerator.midMouseClickButton = mouseButton;

    // chrome.accessibilityPrivate.sendSyntheticMouseEvent only accepts
    // integers.
    x = Math.round(x);
    y = Math.round(y);

    const type = chrome.accessibilityPrivate.SyntheticMouseEventType.PRESS;
    chrome.accessibilityPrivate.sendSyntheticMouseEvent(
        {type, x, y, mouseButton, isDoubleClick});
    return true;
  }

  /**
   * Sends a synthetic mouse release event, if we are mid mouse click.
   * Will start the next click from the click queue if there is one.
   * If we are not mid mouse click, returns false as no release event
   * was sent.
   */
  static sendMouseRelease(x: number, y: number, isDoubleClick = false):
      boolean {
    if (EventGenerator.midMouseClickButton === undefined) {
      return false;
    }

    // chrome.accessibilityPrivate.sendSyntheticMouseEvent only accepts
    // integers.
    x = Math.round(x);
    y = Math.round(y);

    const type = chrome.accessibilityPrivate.SyntheticMouseEventType.RELEASE;
    chrome.accessibilityPrivate.sendSyntheticMouseEvent({
      type,
      x,
      y,
      mouseButton: EventGenerator.midMouseClickButton,
      isDoubleClick,
    });

    EventGenerator.midMouseClickButton = undefined;

    if (EventGenerator.mouseClickQueue.length > 0) {
      const {x, y, params} = EventGenerator.mouseClickQueue.shift()!;
      EventGenerator.sendMouseClick(x, y, params);
    }

    return true;
  }

  /** Sends a synthetic mouse event to simulate a move or drag event. */
  static sendMouseMove(x: number, y: number, touchAccessibility = false): void {
    const type = EventGenerator.midMouseClickButton !== undefined ?
        chrome.accessibilityPrivate.SyntheticMouseEventType.DRAG :
        chrome.accessibilityPrivate.SyntheticMouseEventType.MOVE;
    chrome.accessibilityPrivate.sendSyntheticMouseEvent({
      type,
      x,
      y,
      touchAccessibility,
      mouseButton: EventGenerator.midMouseClickButton,
    });
  }
}

TestImportManager.exportForTesting(EventGenerator);
