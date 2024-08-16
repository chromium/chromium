// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides braille output for ChromeVox.
 * Currently a stub; logic is being moved incrementally from Output to
 * BrailleOutput over a series of small changes.
 */
import {LogType} from '../../common/log_types.js';
import {Spannable} from '../../common/spannable.js';

import {OutputFormatLogger} from './output_logger.js';
import {OutputNodeSpan, OutputSelectionSpan, SPACE} from './output_types.js';

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
