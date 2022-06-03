// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common testing utilities.

/**
 * Shortcut for document.getElementById.
 * @param {string} id of the element.
 * @return {HTMLElement} with the id.
 */
function $(id) {
  return document.getElementById(id);
}

class TestUtils {
  constructor() {}

  /**
   * OBSOLETE: please use multiline string literals. ``.
   * Extracts some inlined html encoded as a comment inside a function,
   * so you can use it like this:
   *
   * this.appendDoc(function() {/*!
   *     <p>Html goes here</p>
   * * /});
   *
   * @param {string|Function} html The html contents. Obsolete support for the
   *     html , embedded as a comment inside an anonymous function - see
   * example, above, still exists.
   * @param {!Array=} opt_args Optional arguments to be substituted in the form
   *     $0, ... within the code block.
   * @return {string} The html text.
   */
  static extractHtmlFromCommentEncodedString(html, opt_args) {
    let stringified = html.toString();
    if (opt_args) {
      for (let i = 0; i < opt_args.length; i++) {
        stringified = stringified.replace('$' + i, opt_args[i]);
      }
    }
    return stringified.replace(/^[^\/]+\/\*!?/, '').replace(/\*\/[^\/]+$/, '');
  }

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
    keyEvent.preventDefault = _ => {};
    keyEvent.stopPropagation = _ => {};
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
      ChromeVox.tts.speak = (textString) => {
        if (textString === textStringToWaitFor) {
          resolve();
        }
      };
    });
  }

  /**
   * Waits for the specified event on the given node.
   * @param {!chrome.automation.AutomationNode} node
   * @param {chrome.automation.EventType} eventType
   * @return {!Promise}
   */
  static waitForEvent(node, eventType) {
    return new Promise(resolve => {
      const listener = () => {
        node.removeEventListener(eventType, listener);
        resolve();
      };
      node.addEventListener(eventType, listener);
    });
  }
}
