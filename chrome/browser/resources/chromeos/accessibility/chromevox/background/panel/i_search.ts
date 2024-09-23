// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The logic behind incremental search.
 */
import {AutomationPredicate} from '/common/automation_predicate.js';
import {AutomationUtil} from '/common/automation_util.js';
import {constants} from '/common/constants.js';
import {Cursor} from '/common/cursors/cursor.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {ISearchHandler} from './i_search_handler.js';

type AutomationNode = chrome.automation.AutomationNode;
import Dir = constants.Dir;

/** Controls an incremental search. */
export class ISearch {
  private callbackId_: number = 0;
  private handler_?: ISearchHandler | null = null;

  cursor: Cursor;

  constructor(cursor: Cursor) {
    if (!cursor.node) {
      throw 'Incremental search started from invalid range.';
    }

    const leaf: AutomationNode = AutomationUtil.findNodePre(
        cursor.node, Dir.FORWARD, AutomationPredicate.leaf) || cursor.node;

    this.cursor = Cursor.fromNode(leaf);
  }

  set handler(handler: ISearchHandler | null) {
    this.handler_ = handler;
  }

  /** Performs a search. */
  search(searchStr: string, dir: Dir, nextObject?: boolean): void {
    clearTimeout(this.callbackId_);
    const step = (): void => {
      searchStr = searchStr.toLocaleLowerCase();
      const node = this.cursor.node;
      let result: AutomationNode | null = node;

      if (nextObject) {
        // We want to start/continue the search at the next object.
        result =
            AutomationUtil.findNextNode(node, dir, AutomationPredicate.object);
      }

      // TODO(b/314203187): Not null asserted, check that this is correct.
      do {
        // Ask native to search the underlying data for a performance boost.
        result = result!.getNextTextMatch(searchStr, dir === Dir.BACKWARD);
      } while (result && !AutomationPredicate.object(result));

      if (result) {
        this.cursor = Cursor.fromNode(result);
        const start = result.name!.toLocaleLowerCase().indexOf(searchStr);
        const end = start + searchStr.length;
        this.handler_!.onSearchResultChanged(result, start, end);
      } else {
        this.handler_!.onSearchReachedBoundary(this.cursor.node);
      }
    };

    this.callbackId_ = setTimeout(() => step(), 0);
  }

  clear(): void {
    clearTimeout(this.callbackId_);
  }
}

TestImportManager.exportForTesting(ISearch);
