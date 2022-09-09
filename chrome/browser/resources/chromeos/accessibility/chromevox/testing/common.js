// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common testing utilities.

class TestUtils {
  /**
   * Create a mock event object.
   * @param {number} keyCode
   * @param {{altGraphKey: boolean=,
   *         altKey: boolean=,
   *         ctrlKey: boolean=,
   *         metaKey: boolean=,
   *         searchKeyHeld: boolean=,
   *         shiftKey: boolean=,
   *         stickyMode: boolean=,
   *         prefixKey: boolean=}=} opt_modifiers
   * @return {Object} The mock event.
   */
  static createMockKeyEvent(keyCode, opt_modifiers) {
    const modifiers = opt_modifiers === undefined ? {} : opt_modifiers;
    const keyEvent = {};
    keyEvent.keyCode = keyCode;
    for (const key in modifiers) {
      keyEvent[key] = modifiers[key];
    }
    keyEvent.preventDefault = () => {};
    keyEvent.stopPropagation = () => {};
    return keyEvent;
  }

  /**
   * Returns a promise which gets resolved when ChromeVox speaks the given
   * string.
   * @param {string} textStringToWaitFor
   * @return {!Promise}
   */
  static waitForSpeech(textStringToWaitFor) {
    return new Promise(resolve => {
      ChromeVox.tts.speak = textString => {
        if (textString === textStringToWaitFor) {
          resolve();
        }
      };
    });
  }
}
