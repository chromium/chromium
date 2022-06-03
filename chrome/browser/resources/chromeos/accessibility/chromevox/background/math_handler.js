// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles math output and exploration.
 */

goog.provide('MathHandler');

/**
 * Initializes math for output and exploration.
 */
MathHandler = class {
  /**
   * @param {!chrome.automation.AutomationNode} node
   */
  constructor(node) {
    /** @private {!chrome.automation.AutomationNode} */
    this.node_ = node;
  }

  /**
   * Speaks the current node.
   * @return {boolean} Whether any math was spoken.
   */
  speak() {
    let mathml;

    // Math can exist either as explicit innerHtml (handled by the Blink
    // renderer for nodes with role math) or as a data attribute.
    if (this.node_.role === chrome.automation.RoleType.MATH &&
        this.node_.innerHtml) {
      mathml = this.node_.innerHtml;
    } else {
      mathml = this.node_.htmlAttributes['data-mathml'];
    }

    if (!mathml) {
      return false;
    }

    let text;

    try {
      text = SRE.walk(mathml);
    } catch (e) {
      // Swallow exceptions from third party library.
    }

    if (!text) {
      return false;
    }

    ChromeVox.tts.speak(text, QueueMode.FLUSH);
    ChromeVox.tts.speak(Msgs.getMsg('hint_math_keyboard'), QueueMode.QUEUE);
    return true;
  }

  /**
   * Initializes the global instance.
   * @param {cursors.Range} range
   * @return {boolean} True if an instance was created.
   */
  static init(range) {
    const node = range.start.node;
    if (node && AutomationPredicate.math(node)) {
      MathHandler.instance = new MathHandler(node);
    } else {
      MathHandler.instance = undefined;
    }
    return !!MathHandler.instance;
  }

  /**
   * Handles key events.
   * @return {boolean} False to prevent further event propagation.
   */
  static onKeyDown(evt) {
    if (!MathHandler.instance) {
      return true;
    }

    if (evt.ctrlKey || evt.altKey || evt.metaKey || evt.shiftKey ||
        evt.stickyMode) {
      return true;
    }

    const instance = MathHandler.instance;
    const output = SRE.move(evt.keyCode);
    if (output) {
      ChromeVox.tts.speak(output, QueueMode.FLUSH);
    }
    return false;
  }
};


/**
 * The global instance.
 * @type {MathHandler|undefined}
 */
MathHandler.instance;
