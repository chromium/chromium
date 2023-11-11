// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NavBraille} from '../../common/braille/nav_braille.js';
import {Msgs} from '../../common/msgs.js';
import {Spannable} from '../../common/spannable.js';
import {ValueSelectionSpan, ValueSpan} from '../braille/spans.js';
import {ChromeVox} from '../chromevox.js';
import {OutputNodeSpan} from '../output/output_types.js';

import {ChromeVoxEditableTextBase, TextChangeEvent} from './editable_text_base.js';

const AutomationIntent = chrome.automation.AutomationIntent;
const AutomationNode = chrome.automation.AutomationNode;
const StateType = chrome.automation.StateType;

/**
 * A |ChromeVoxEditableTextBase| that implements text editing feedback
 * for automation tree text fields.
 */
export class AutomationEditableText extends ChromeVoxEditableTextBase {
  /** @param {!AutomationNode} node */
  constructor(node) {
    if (!node.state.editable) {
      throw Error('Node must have editable state set to true.');
    }
    const value = AutomationEditableText.getProcessedValue_(node) ?? '';
    const lineBreaks = AutomationEditableText.getLineBreaks_(value);
    const start = node.textSelStart;
    const end = node.textSelEnd;

    super(
        value, Math.min(start, end, value.length),
        Math.min(Math.max(start, end), value.length),
        node.state[StateType.PROTECTED] /**password*/, ChromeVox.tts);
    /** @private {!Array<number>} */
    this.lineBreaks_ = lineBreaks;
    /** @override */
    this.multiline = node.state[StateType.MULTILINE] || false;
    /** @protected {!AutomationNode} */
    this.node_ = node;
  }

  /**
   * Update the state of the text and selection and describe any changes as
   * appropriate.
   *
   * @param {TextChangeEvent} evt The text change event.
   */
  changed(evt) {
    if (!this.shouldDescribeChange(evt)) {
      this.lastChangeDescribed = false;
      return;
    }

    if (evt.value === this.value) {
      this.describeSelectionChanged(evt);
    } else {
      this.describeTextChanged(
          new TextChangeEvent(this.value, this.start, this.end, true), evt);
    }
    this.lastChangeDescribed = true;

    this.value = evt.value;
    this.start = evt.start;
    this.end = evt.end;
  }

  /**
   * Called when the text field has been updated.
   * @param {!Array<AutomationIntent>} intents
   */
  onUpdate(intents) {
    const oldValue = this.value;
    const oldStart = this.start;
    const oldEnd = this.end;
    const newValue =
        AutomationEditableText.getProcessedValue_(this.node_) ?? '';
    if (oldValue !== newValue) {
      this.lineBreaks_ = AutomationEditableText.getLineBreaks_(newValue);
    }

    const textChangeEvent = new TextChangeEvent(
        newValue, Math.min(this.node_.textSelStart ?? 0, newValue.length),
        Math.min(this.node_.textSelEnd ?? 0, newValue.length),
        true /* triggered by user */);
    this.changed(textChangeEvent);
    this.outputBraille_(oldValue, oldStart, oldEnd);
  }

  /**
   * Returns true if selection starts on the first line.
   */
  isSelectionOnFirstLine() {
    return this.getLineIndex(this.start) === 0;
  }

  /**
   * Returns true if selection ends on the last line.
   */
  isSelectionOnLastLine() {
    return this.getLineIndex(this.end) >= this.lineBreaks_.length - 1;
  }

  /** @override */
  getLineIndex(charIndex) {
    let lineIndex = 0;
    while (charIndex > this.lineBreaks_[lineIndex]) {
      lineIndex++;
    }
    return lineIndex;
  }

  /** @override */
  getLineStart(lineIndex) {
    if (lineIndex === 0) {
      return 0;
    }

    // The start of this line is defined as the line break of the previous line
    // + 1 (the hard line break).
    return this.lineBreaks_[lineIndex - 1] + 1;
  }

  /** @override */
  getLineEnd(lineIndex) {
    return this.lineBreaks_[lineIndex];
  }

  /** @private */
  getLineIndexForBrailleOutput_(oldStart) {
    let lineIndex = this.getLineIndex(this.start);
    // Output braille at the end of the selection that changed, if start and end
    // differ.
    if (this.start !== this.end && this.start === oldStart) {
      lineIndex = this.getLineIndex(this.end);
    }
    return lineIndex;
  }

  /** @private */
  getTextFromIndexAndStart_(lineIndex, lineStart) {
    const lineEnd = this.getLineEnd(lineIndex);
    let lineText = this.value.substr(lineStart, lineEnd - lineStart);

    if (lineIndex === 0) {
      const textFieldTypeMsg =
          Msgs.getMsg(this.multiline ? 'tag_textarea_brl' : 'role_textbox_brl');
      lineText += ' ' + textFieldTypeMsg;
    }

    return lineText;
  }

  /** @private */
  outputBraille_(oldValue, oldStart, oldEnd) {
    const lineIndex = this.getLineIndexForBrailleOutput_(oldStart);
    const lineStart = this.getLineStart(lineIndex);
    let lineText = this.getTextFromIndexAndStart_(lineIndex, lineStart);

    const startIndex = this.start - lineStart;
    const endIndex = this.end - lineStart;

    // If the line is not the last line, and is empty, insert an explicit line
    // break so that braille output is correctly cleared and has a position for
    // a caret to be shown.
    if (lineText === '' && lineIndex < this.lineBreaks_.length - 1) {
      lineText = '\n';
    }

    const value = new Spannable(lineText, new OutputNodeSpan(this.node_));
    value.setSpan(new ValueSpan(0), 0, lineText.length);
    value.setSpan(new ValueSelectionSpan(), startIndex, endIndex);
    ChromeVox.braille.write(
        new NavBraille({text: value, startIndex, endIndex}));
  }

  /**
   * @param {!AutomationNode} node
   * @return {string|undefined}
   * @private
   */
  static getProcessedValue_(node) {
    let value = node.value;
    if (node.inputType === 'tel') {
      value = value?.trimEnd();
    }
    return value;
  }

  /**
   * @param {string} value
   * @return {!Array<number>}
   * @private
   */
  static getLineBreaks_(value) {
    const lineBreaks = [];
    const lines = value.split('\n');
    let total = 0;
    for (let i = 0; i < lines.length; i++) {
      total += lines[i].length;
      lineBreaks[i] = total;

      // Account for the line break itself.
      total++;
    }
    return lineBreaks;
  }
}
