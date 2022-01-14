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

    /** @private {!Date} */
    this.lastScrolledTime_ = new Date(0);

    /** @private {boolean} */
    this.relatedFocusEventHappened_ = false;
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
   * @param {AutomationPredicate.Unary} rootPred The predicate that expresses
   *     the current navigation root.
   * @param {Function} retryCommandFunc The callback used to retry the command
   *     with refreshed tree after scrolling.
   * @return {boolean} True if the given navigation can be executed. False if
   *     the given navigation shouldn't happen, and AutoScrollHandler handles
   *     the command instead.
   */
  onCommandNavigation(
      target, dir, pred, speechProps, rootPred, retryCommandFunc) {
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
    this.lastScrolledTime_ = new Date();
    this.relatedFocusEventHappened_ = false;

    const rangeBeforeScroll = ChromeVoxState.instance.currentRange;

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
      // TODO(hirokisato): On large page scrolling in Android, the
      // SCROLL_POSITION_CHANGED event is dispatched multiple times, and if
      // we only listen the first event, there is a chance that the ui tree
      // is intermediate state and we set the focus on the inappropriate
      // node. Consider waiting until the UI gets stabilized.
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

      // This block handles scrolling on Android RecyclerView.
      // When scrolling happens, RecyclerView can delete an invisible item,
      // which might be the focused node before scrolling, or can re-use the
      // focused node for a newly added node. When one of these happens, the
      // focused node first disappears from the ARC tree and the focus is moved
      // up to the View that has the ARC tree. In this case, navigat to the
      // first or the last matching range in the scrollable.
      if (this.relatedFocusEventHappened_) {
        let nextRange = null;
        if (!pred) {
          // Currently this only supports nextObject/previousObject.
          // TODO(hirokisato): Support navigation by unit.
          pred = AutomationPredicate.object;
        }
        let node;
        if (dir === Dir.FORWARD) {
          node = AutomationUtil.findNextNode(
              this.scrollingNode_, dir, pred,
              {root: rootPred, skipInitialSubtree: false});
        } else {
          node = AutomationUtil.findNodePost(this.scrollingNode_, dir, pred);
        }
        if (node) {
          nextRange = cursors.Range.fromNode(node);
        }

        ChromeVoxState.instance.navigateToRange(
            nextRange || target, false, speechProps);
        return;
      }

      // If the focus has been changed for some reason, do nothing to
      // prevent disturbing the latest navigation.
      if (!rangeBeforeScroll.equals(ChromeVoxState.instance.currentRange)) {
        return;
      }

      // Usual case. Retry navigation with a refreshed tree.
      retryCommandFunc();
    })().catch(e => {
      this.isScrolling_ = false;
    });

    return false;
  }

  /**
   * This should be called before the focus event triggers the navigation.
   *
   * On scrolling, sometimes previously focused node disappears.
   * There are two common problematic patterns in ARC tree here:
   * 1. No node has focus. The root ash window now get focus.
   * 2. Another node in the window gets focus.
   * Above two cases interrupt the auto scrolling. So when any of these are
   * happening, this returns false.
   *
   * @param {AutomationNode} node the node that is the focus event target.
   * @return {boolean} True if others can handle the event. False if the
   *     event shouldn't be propagated.
   */
  onFocusEventNavigation(node) {
    if (!this.scrollingNode_ || !node) {
      return true;
    }
    const elapsedTime = new Date() - this.lastScrolledTime_;
    if (elapsedTime > AutoScrollHandler.TIMEOUT_FOCUS_EVENT_DROP_MS) {
      return true;
    }

    const isUnrelatedEvent = this.scrollingNode_.root !== node.root &&
        !AutomationUtil.isDescendantOf(this.scrollingNode_, node);
    if (isUnrelatedEvent) {
      return true;
    }

    this.relatedFocusEventHappened_ = true;
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

/**
 * The timeout that the focused event should be dropped. This is longer than
 * |TIMEOUT_CALLBACK_MS| because a focus event can happen after the scrolling.
 * @const {number}
 */
AutoScrollHandler.TIMEOUT_FOCUS_EVENT_DROP_MS = 2000;

goog.addSingletonGetter(AutoScrollHandler);
});  // goog.scope
