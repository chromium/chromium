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

type AutomationIntent = chrome.automation.AutomationIntent;
type AutomationNode = chrome.automation.AutomationNode;
const StateType = chrome.automation.StateType;

/**
 * A |ChromeVoxEditableTextBase| that implements text editing feedback
 * for automation tree text fields.
 */
export class AutomationEditableText extends ChromeVoxEditableTextBase {
  private lineBreaks_: number[];
  protected node_: AutomationNode;

  constructor(node: AutomationNode) {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (!node.state![StateType.EDITABLE]) {
      throw Error('Node must have editable state set to true.');
    }
    const value = AutomationEditableText.getProcessedValue_(node) ?? '';
    const lineBreaks = AutomationEditableText.getLineBreaks_(value);
    const start = node.textSelStart!;
    const end = node.textSelEnd!;

    super(
        value, Math.min(start, end, value.length),
        Math.min(Math.max(start, end), value.length),
        node.state![StateType.PROTECTED] /**password*/, ChromeVox.tts);
    this.lineBreaks_ = lineBreaks;
    this.multiline = node.state![StateType.MULTILINE] || false;
    this.node_ = node;
  }

  /** Called when the text field has been updated. */
  onUpdate(_intents: AutomationIntent[]): void {
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

  /** Returns true if selection starts on the first line. */
  isSelectionOnFirstLine(): boolean {
    return this.getLineIndex(this.start) === 0;
  }

  /** Returns true if selection ends on the last line. */
  isSelectionOnLastLine(): boolean {
    return this.getLineIndex(this.end) >= this.lineBreaks_.length - 1;
  }

  override getLineIndex(charIndex: number): number {
    let lineIndex = 0;
    while (charIndex > this.lineBreaks_[lineIndex]) {
      lineIndex++;
    }
    return lineIndex;
  }

  override getLineStart(lineIndex: number): number {
    if (lineIndex === 0) {
      return 0;
    }

    // The start of this line is defined as the line break of the previous line
    // + 1 (the hard line break).
    return this.lineBreaks_[lineIndex - 1] + 1;
  }

  override getLineEnd(lineIndex: number): number {
    return this.lineBreaks_[lineIndex];
  }

  private getLineIndexForBrailleOutput_(oldStart: number): number {
    let lineIndex = this.getLineIndex(this.start);
    // Output braille at the end of the selection that changed, if start and end
    // differ.
    if (this.start !== this.end && this.start === oldStart) {
      lineIndex = this.getLineIndex(this.end);
    }
    return lineIndex;
  }

  private getTextFromIndexAndStart_(
      lineIndex: number, lineStart: number): string {
    const lineEnd = this.getLineEnd(lineIndex);
    let lineText = this.value.substr(lineStart, lineEnd - lineStart);

    if (lineIndex === 0) {
      const textFieldTypeMsg =
          Msgs.getMsg(this.multiline ? 'tag_textarea_brl' : 'role_textbox_brl');
      lineText += ' ' + textFieldTypeMsg;
    }

    return lineText;
  }

  private outputBraille_(
      _oldValue: string, oldStart: number, _oldEnd: number): void {
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

  private static getProcessedValue_(node: AutomationNode): string|undefined {
    let value = node.value;
    if (node.inputType === 'tel') {
      value = value?.trimEnd();
    }
    return value;
  }

  private static getLineBreaks_(value: string): number[] {
    const lineBreaks: number[] = [];
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
