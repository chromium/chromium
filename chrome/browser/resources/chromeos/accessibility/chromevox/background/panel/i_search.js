// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The logic behind incremental search.
 */
import {AutomationPredicate} from '../../../common/automation_predicate.js';
import {AutomationUtil} from '../../../common/automation_util.js';
import {constants} from '../../../common/constants.js';
import {Cursor} from '../../../common/cursors/cursor.js';

import {ISearchHandler} from './i_search_handler.js';

const Dir = constants.Dir;

/** Controls an incremental search. */
export class ISearch {
  /** @param {!Cursor} cursor */
  constructor(cursor) {
    if (!cursor.node) {
      throw 'Incremental search started from invalid range.';
    }

    /** @private {?ISearchHandler} */
    this.handler_ = null;

    const leaf = AutomationUtil.findNodePre(
                     cursor.node, Dir.FORWARD, AutomationPredicate.leaf) ||
        cursor.node;

    /** @type {!Cursor} */
    this.cursor = Cursor.fromNode(leaf);

    /** @private {number} */
    this.callbackId_ = 0;
  }

  /** @param {?ISearchHandler} handler */
  set handler(handler) {
    this.handler_ = handler;
  }

  /**
   * Performs a search.
   * @param {string} searchStr
   * @param {constants.Dir} dir
   * @param {boolean=} opt_nextObject
   */
  search(searchStr, dir, opt_nextObject) {
    clearTimeout(this.callbackId_);
    const step = () => {
      searchStr = searchStr.toLocaleLowerCase();
      const node = this.cursor.node;
      let result = node;

      if (opt_nextObject) {
        // We want to start/continue the search at the next object.
        result =
            AutomationUtil.findNextNode(node, dir, AutomationPredicate.object);
      }

      do {
        // Ask native to search the underlying data for a performance boost.
        result = result.getNextTextMatch(searchStr, dir === Dir.BACKWARD);
      } while (result && !AutomationPredicate.object(result));

      if (result) {
        this.cursor = Cursor.fromNode(result);
        const start = result.name.toLocaleLowerCase().indexOf(searchStr);
        const end = start + searchStr.length;
        this.handler_.onSearchResultChanged(result, start, end);
      } else {
        this.handler_.onSearchReachedBoundary(this.cursor.node);
      }
    };

    this.callbackId_ = setTimeout(() => step(), 0);
  }

  clear() {
    clearTimeout(this.callbackId_);
  }
}
