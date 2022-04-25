// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class that incrementally searches the ChromeVox menus.
 */
export class ISearch {
  /**
   * @param {!cursors.Cursor} cursor
   */
  constructor(cursor) {
    if (!cursor.node) {
      throw 'Incremental search started from invalid range.';
    }

    const leaf =
        AutomationUtil.findNodePre(
            cursor.node, constants.Dir.FORWARD, AutomationPredicate.leaf) ||
        cursor.node;

    /** @type {!cursors.Cursor} */
    this.cursor = cursors.Cursor.fromNode(leaf);

    /** @private {number} */
    this.callbackId_ = 0;
  }

  /**
   * Performs a search.
   * @param {string} searchStr
   * @param {constants.Dir} dir
   * @param {boolean=} opt_nextObject
   */
  search(searchStr, dir, opt_nextObject) {
    this.clear();
    const step = () => {
      searchStr = searchStr.toLocaleLowerCase();
      let result = this.cursor.node;

      if (opt_nextObject) {
        // We want to start/continue the search at the next object.
        result = AutomationUtil.findNextNode(
            this.cursor.node, dir, AutomationPredicate.object);
      }

      do {
        // Ask native to search the underlying data for a performance boost.
        result =
            result.getNextTextMatch(searchStr, dir === constants.Dir.BACKWARD);
      } while (result && !AutomationPredicate.object(result));

      if (result) {
        this.cursor = cursors.Cursor.fromNode(result);
        const start = result.name.toLocaleLowerCase().indexOf(searchStr);
        const end = start + searchStr.length;
        this.onSearchResultChanged_(result, start, end);
      } else {
        this.onSearchReachedBoundary_(this.cursor.node);
      }
    };

    this.callbackId_ = setTimeout(step, 0);
  }

  clear() {
    clearTimeout(this.callbackId_);
  }

  /**
   * @param {!chrome.automation.AutomationNode} boundaryNode
   * @private
   */
  onSearchReachedBoundary_(boundaryNode) {
    this.output_(boundaryNode);
    ChromeVox.earcons.playEarcon(Earcon.WRAP);
  }

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @param {number} start
   * @param {number} end
   * @private
   */
  onSearchResultChanged_(node, start, end) {
    this.output_(node, start, end);
  }

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @param {number=} opt_start
   * @param {number=} opt_end
   * @private
   */
  output_(node, opt_start, opt_end) {
    Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);
    const o = new Output();
    if (opt_start && opt_end) {
      o.withString([
        node.name.substr(0, opt_start),
        node.name.substr(opt_start, opt_end - opt_start),
        node.name.substr(opt_end)
      ].join(', '));
      o.format('$role', node);
    } else {
      o.withRichSpeechAndBraille(
          cursors.Range.fromNode(node), null, OutputEventType.NAVIGATE);
    }
    o.go();

    ChromeVoxState.instance.setCurrentRange(cursors.Range.fromNode(node));
  }
}
