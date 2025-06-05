// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles math output and exploration.
 */
import {AutomationPredicate} from '/common/automation_predicate.js';
import type {CursorRange} from '/common/cursors/range.js';

import {Msgs} from '../common/msgs.js';
import {QueueMode} from '../common/tts_types.js';

import {ChromeVox} from './chromevox.js';
import type {InternalKeyEvent} from './input/background_keyboard_handler.js';

import AutomationNode = chrome.automation.AutomationNode;

// Speech Rule Engine is included as a global variable in background.html.
declare let SRE: any;

/**
 * Handles specialized code to navigate, announce, and interact with math
 * content (encoded in MathML).
 */
export class MathHandler {
  private node_: AutomationNode;

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

    ChromeVox.tts.speak(text, QueueMode.FLUSH);
    ChromeVox.tts.speak(Msgs.getMsg('hint_math_keyboard'), QueueMode.QUEUE);
    return true;
  }

  /**
   * Initializes the global instance.
   * @return Boolean indicating whether an instance was created.
   */
  static init(range: CursorRange): boolean {
    const node = range.start.node;
    if (node && AutomationPredicate.math(node)) {
      MathHandler.instance = new MathHandler(node);
    } else {
      MathHandler.instance = undefined;
    }
    return Boolean(MathHandler.instance);
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
