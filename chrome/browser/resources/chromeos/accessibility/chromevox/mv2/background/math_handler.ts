// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles math output and exploration.
 */
import {SRE} from '/chromevox/mv2/third_party/sre/sre_browser.js';
import {AutomationPredicate} from '/common/automation_predicate.js';
import type {CursorRange} from '/common/cursors/range.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Msgs} from '../common/msgs.js';
import {QueueMode} from '../common/tts_types.js';

import {ChromeVox} from './chromevox.js';
import type {InternalKeyEvent} from './input/background_keyboard_handler.js';
import {ChromeVoxPrefs} from './prefs.js';

import AutomationNode = chrome.automation.AutomationNode;

/**
 * Handles specialized code to navigate, announce, and interact with math
 * content (encoded in MathML).
 */
export class MathHandler {
  private node_: AutomationNode;
  private capturing_ = false;

  static instance: MathHandler|undefined = undefined;

  private constructor(node: AutomationNode) {
    this.node_ = node;
  }

  /**
   * Speaks the current node.
   * @return Boolean indicating whether any math was spoken.
   */
  speak(): boolean {
    let mathml = this.node_.mathContent;
    if (!mathml) {
      return false;
    }
    // Ensure it has a `math` root node.
    if (!/^<math>${mathml}<\/math>$/.test(mathml)) {
      mathml = '<math>' + mathml + '</math>';
    }

    let text: string|null = null;

    try {
      text = SRE.walk(mathml);
    } catch (e) {
      // Swallow exceptions from third party library.
    }

    if (!text) {
      return false;
    }

    // Ensure we are capturing key events once we reach the math node, so that
    // arrow keys will be captured. Note that if sticky mode is on, we are
    // already capturing these events, and this will have no effect. If sticky
    // mode is off, we now capture.
    this.capturing_ = true;
    chrome.accessibilityPrivate.setKeyboardListener(true, true);

    ChromeVox.tts.speak(text, QueueMode.FLUSH);
    ChromeVox.tts.speak(Msgs.getMsg('hint_math_keyboard'), QueueMode.QUEUE);
    return true;
  }

  isCapturing(): boolean {
    return this.capturing_;
  }

  node(): AutomationNode {
    return this.node_;
  }

  /**
   * Initializes the global instance based on the current cursor range,
   * if it is a Math node.
   * @return Boolean indicating whether an instance was created.
   */
  static init(range: CursorRange): boolean {
    const node = range.start.node;
    if (node && AutomationPredicate.math(node)) {
      MathHandler.instance = new MathHandler(node);
    } else if (MathHandler.instance !== undefined) {
      MathHandler.instance = undefined;
    }
    return Boolean(MathHandler.instance);
  }

  /**
   * Ensures the MathHandler instance is still valid after moving to the current
   * cursor range. If not, ensures that the keyboard listener is cleared.
   * @param range The current cursor range
   */
  static checkInstance(range: CursorRange|null): void {
    if (!MathHandler.instance) {
      return;
    }
    if (!range || MathHandler.instance!.node() !== range.start.node ||
        range.start.node !== range.end.node) {
      MathHandler.instance = undefined;
      // Ensure we are no longer capturing key events unless sticky mode is on.
      chrome.accessibilityPrivate.setKeyboardListener(
          true, ChromeVoxPrefs.isStickyPrefOn);
    }
  }

  /**
   * Handles key events.
   * @return Boolean indicating whether an event should propagate.
   */
  static onKeyDown(evt: InternalKeyEvent): boolean {
    if (!MathHandler.instance) {
      return true;
    }

    if (evt.ctrlKey || evt.altKey || evt.metaKey || evt.shiftKey ||
        evt.stickyMode) {
      return true;
    }

    const output: string = SRE.move(evt.keyCode);
    if (output) {
      ChromeVox.tts.speak(output, QueueMode.FLUSH);
    }
    return false;
  }
}

TestImportManager.exportForTesting(MathHandler);
