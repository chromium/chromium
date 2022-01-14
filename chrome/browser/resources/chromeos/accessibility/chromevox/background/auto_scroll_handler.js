// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles auto scrolling on navigation.
 */

goog.provide('AutoScrollHandler');

goog.require('AutomationPredicate');
goog.require('AutomationUtil');
goog.require('constants');

goog.scope(function() {
const Dir = constants.Dir;
const EventType = chrome.automation.EventType;

/**
 * Handler of auto scrolling. Most logics are for supporting ARC++.
 */
AutoScrollHandler = class {
  constructor() {}

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
    const callback = function(result) {
      if (!result) {
        ChromeVoxState.instance.navigateToRange(target, false, speechProps);
        return;
      }

      const innerCallback = function(currentNode, evt) {
        scrollable.removeEventListener(
            EventType.SCROLL_POSITION_CHANGED, innerCallback);
        scrollable.removeEventListener(
            EventType.SCROLL_HORIZONTAL_POSITION_CHANGED, innerCallback);
        scrollable.removeEventListener(
            EventType.SCROLL_VERTICAL_POSITION_CHANGED, innerCallback);

        if (pred || (currentNode && currentNode.root)) {
          // Jump or if there is a valid current range, then move from it
          // since we have refreshed node data.
          retryCommandFunc();
          return;
        }

        // Otherwise, sync to the directed deepest child.
        let sync = scrollable;
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
      }.bind(this, target.start.node);
      // This is sent by ARC++.
      scrollable.addEventListener(
          EventType.SCROLL_POSITION_CHANGED, innerCallback, true);
      // These two events are sent by Web and Views via AXEventGenerator.
      scrollable.addEventListener(
          EventType.SCROLL_HORIZONTAL_POSITION_CHANGED, innerCallback, true);
      scrollable.addEventListener(
          EventType.SCROLL_VERTICAL_POSITION_CHANGED, innerCallback, true);
    };

    if (dir === Dir.FORWARD) {
      scrollable.scrollForward(callback);
    } else {
      scrollable.scrollBackward(callback);
    }
    return false;
  }
};

goog.addSingletonGetter(AutoScrollHandler);
});  // goog.scope
