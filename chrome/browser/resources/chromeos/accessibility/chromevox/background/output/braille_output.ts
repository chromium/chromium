// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides braille output for ChromeVox.
 * Currently a stub; logic is being moved incrementally from Output to
 * BrailleOutput over a series of small changes.
 */

import {CursorRange} from '/common/cursors/range.js';

import {LogType} from '../../common/log_types.js';
import {Spannable} from '../../common/spannable.js';

import {OutputFormatLogger} from './output_logger.js';
import {AppendOptions, OutputNodeSpan, OutputSelectionSpan, SPACE} from './output_types.js';

type AutomationNode = chrome.automation.AutomationNode;
import RoleType = chrome.automation.RoleType;

export class BrailleOutput {
  readonly buffer: Spannable[] = [];
  readonly formatLog =
      new OutputFormatLogger('enableBrailleLogging', LogType.BRAILLE_RULE);

  equals(rhs: BrailleOutput): boolean {
    if (this.buffer.length !== rhs.buffer.length) {
      return false;
    }

    for (let i = 0; i < this.buffer.length; i++) {
      if (this.buffer[i].toString() !== rhs.buffer[i].toString()) {
        return false;
      }
    }
    return true;
  }

  subNode(range: CursorRange, options: AppendOptions): AppendOptions {
    const node = range.start.node;
    const rangeStart = range.start.index;
    const rangeEnd = range.end.index;

    options.annotation.push(new OutputNodeSpan(node));
    // TODO(b/314203187): Not null asserted, check that this is correct.
    const selStart = node.textSelStart!;
    const selEnd = node.textSelEnd!;

    if (selStart !== undefined && selEnd >= rangeStart &&
        selStart <= rangeEnd) {
      // Editable text selection.

      // |rangeStart| and |rangeEnd| are indices set by the caller and are
      // assumed to be inside of the range. In braille, we only ever expect
      // to get ranges surrounding a line as anything smaller doesn't make
      // sense.

      // |selStart| and |selEnd| reflect the editable selection. The
      // relative selStart and relative selEnd for the current line are then
      // just the difference between |selStart|, |selEnd| with |rangeStart|.
      // See editing_test.js for examples.
      options.annotation.push(
          new OutputSelectionSpan(selStart - rangeStart, selEnd - rangeStart));
    } else if (rangeStart !== 0 || rangeEnd !== range.start.getText().length) {
      // Non-editable text selection over less than the full contents
      // covered by the range. We exclude full content underlines because it
      // is distracting to read braille with all cells underlined with a
      // cursor.
      options.annotation.push(new OutputSelectionSpan(rangeStart, rangeEnd));
    }

    return options;
  }


  /** Converts the braille |spans| buffer to a single spannable. */
  static mergeSpans(spans: Spannable[]): Spannable {
    let separator = '';  // Changes to space as appropriate.
    let prevHasInlineNode = false;
    let prevIsName = false;
    return spans.reduce((result, cur) => {
      // Ignore empty spans except when they contain a selection.
      const hasSelection = cur.getSpanInstanceOf(OutputSelectionSpan);
      if (cur.length === 0 && !hasSelection) {
        return result;
      }

      // For empty selections, we just add the space separator to account for
      // showing the braille cursor.
      if (cur.length === 0 && hasSelection) {
        result.append(cur);
        result.append(SPACE);
        separator = '';
        return result;
      }

      // Keep track of if there's an inline node associated with
      // |cur|.
      const hasInlineNode =
          cur.getSpansInstanceOf(OutputNodeSpan)
              .some((spannableObj: Object) => {
                const spannable: {node: AutomationNode} =
                    spannableObj as {node: AutomationNode};
                if (!spannable.node) {
                  return false;
                }
                return spannable.node.display === 'inline' ||
                    spannable.node.role === RoleType.INLINE_TEXT_BOX;
              });

      const isName = cur.hasSpan('name');

      // Now, decide whether we should include separators between the previous
      // span and |cur|.
      // Never separate chunks without something already there at this point.

      // The only case where we know for certain that a separator is not
      // needed is when the previous and current values are in-lined and part
      // of the node's name. In all other cases, use the surrounding
      // whitespace to ensure we only have one separator between the node
      // text.
      if (result.length === 0 ||
          (hasInlineNode && prevHasInlineNode && isName && prevIsName)) {
        separator = '';
      } else if (
          result.toString()[result.length - 1] === SPACE ||
          cur.toString()[0] === SPACE) {
        separator = '';
      } else {
        separator = SPACE;
      }

      prevHasInlineNode = hasInlineNode;
      prevIsName = isName;
      result.append(separator);
      result.append(cur);
      return result;
    }, new Spannable());
  }
}
