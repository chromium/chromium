// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles auto scrolling on navigation.
 */

// setTimeout and its clean-up are referencing each other. So, we need to set
// "ignoreReadBeforeAssign" in this file. ESLint doesn't support per-line rule
// option modification.
/* eslint prefer-const: ["error", {"ignoreReadBeforeAssign": true}] */

goog.provide('AutoScrollHandler');

goog.require('AutomationPredicate');
goog.require('AutomationUtil');
goog.require('constants');

goog.scope(function() {
const Dir = constants.Dir;
const AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;

/**
 * Handler of auto scrolling. Most logics are for supporting ARC++.
 */
AutoScrollHandler = class {
  constructor() {
    /** @private {boolean} */
    this.isScrolling_ = false;

    /** @private {AutomationNode} */
    this.scrollingNode_ = null;
  }

  /**
   * This should be called before any command triggers ChromeVox navigation.
   *
   * @param {!cursors.Range} target The range that is going to be navigated
   *     before scrolling.
   * @param {Dir} dir The direction to navigate.
   * @param {?AutomationPredicate.Unary} pred The predicate to match.
   * @param {?Object} speechProps The optional speech properties given to
   *     |navigateToRange| to provide feedback of the current command.
   * @param {Function} retryCommandFunc The callback used to retry the command
   *     with refreshed tree after scrolling.
   * @return {boolean} True if the given navigation can be executed. False if
   *     the given navigation shouldn't happen, and AutoScrollHandler handles
   *     the command instead.
   */
  onCommandNavigation(target, dir, pred, speechProps, retryCommandFunc) {
    if (this.isScrolling_) {
      // Prevent interrupting the current scrolling.
      return false;
    }

    if (!target.start || !target.start.node ||
        !ChromeVoxState.instance.currentRange.start.node) {
      return true;
    }

    const exited = AutomationUtil.getUniqueAncestors(
        target.start.node, ChromeVoxState.instance.currentRange.start.node);
    let scrollable = null;
    for (let i = 0; i < exited.length; i++) {
      if (AutomationPredicate.autoScrollable(exited[i])) {
        scrollable = exited[i];
        break;
      }
    }

    if (!scrollable) {
      return true;
    }

    // TODO(crbug/761415): handle more precise positioning after scroll.
    // e.g. list with 10 items showing 1-7, scroll forward, should position at
    // item 8.
    this.isScrolling_ = true;
    this.scrollingNode_ = scrollable;

    (async () => {
      const scrollResult = await new Promise(resolve => {
        if (dir === Dir.FORWARD) {
          scrollable.scrollForward(resolve);
        } else {
          scrollable.scrollBackward(resolve);
        }
      });
      if (!scrollResult) {
        ChromeVoxState.instance.navigateToRange(target, false, speechProps);
        return;
      }

      // Wait for a scrolled event or timeout.
      await new Promise((resolve, reject) => {
        let cleanUp;
        let timeoutId;
        const onTimeout = () => {
          cleanUp();
          reject('timeout to wait for scrolled event.');
        };
        const onScrolled = () => {
          cleanUp();
          resolve();
        };
        cleanUp = () => {
          for (const e of AutoScrollHandler.RELATED_SCROLL_EVENT_TYPES) {
            scrollable.removeEventListener(e, onScrolled, true);
          }
          clearTimeout(timeoutId);
        };

        for (const e of AutoScrollHandler.RELATED_SCROLL_EVENT_TYPES) {
          scrollable.addEventListener(e, onScrolled, true);
        }
        timeoutId =
            setTimeout(onTimeout, AutoScrollHandler.TIMEOUT_SCROLLED_EVENT_MS);
      });

      this.isScrolling_ = false;

      if (!this.scrollingNode_) {
        throw Error(
            'Illegal state in AutoScrollHandler. |scrollingNode_| is null.');
      }

      if (pred || (target.start.node && target.start.node.root)) {
        // Jump or if there is a valid current range, then move from it
        // since we have refreshed node data.
        retryCommandFunc();
        return;
      }

      // Otherwise, sync to the directed deepest child.
      let sync = this.scrollingNode_;
      if (dir === Dir.FORWARD) {
        while (sync.firstChild) {
          sync = sync.firstChild;
        }
      } else {
        while (sync.lastChild) {
          sync = sync.lastChild;
        }
      }
      ChromeVoxState.instance.navigateToRange(
          cursors.Range.fromNode(sync), false, speechProps);
    })().catch(e => {
      this.isScrolling_ = false;
    });

    return false;
  }
};

/**
 * An array of Automation event types that AutoScrollHandler observes when
 * performing a scroll action.
 * @private
 * @const {!Array<EventType>}
 */
AutoScrollHandler.RELATED_SCROLL_EVENT_TYPES = [
  // This is sent by ARC++.
  EventType.SCROLL_POSITION_CHANGED,
  // These two events are sent by Web and Views via AXEventGenerator.
  EventType.SCROLL_HORIZONTAL_POSITION_CHANGED,
  EventType.SCROLL_VERTICAL_POSITION_CHANGED
];

/**
 * The timeout that we wait for scroll event since scrolling action's callback
 * is executed.
 * TODO(hirokisato): Find an appropriate timeout in our case. TalkBack uses 500
 * milliseconds. See
 * https://github.com/google/talkback/blob/acd0bc7631a3dfbcf183789c7557596a45319e1f/talkback/src/main/java/ScrollEventInterpreter.java#L169
 * @const {number}
 */
AutoScrollHandler.TIMEOUT_SCROLLED_EVENT_MS = 1500;

goog.addSingletonGetter(AutoScrollHandler);
});  // goog.scope
