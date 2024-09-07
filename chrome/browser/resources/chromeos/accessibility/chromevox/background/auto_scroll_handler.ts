// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles auto scrolling on navigation.
 */
import {AutomationPredicate} from '/common/automation_predicate.js';
import {AutomationUtil} from '/common/automation_util.js';
import {constants} from '/common/constants.js';
import {CursorUnit} from '/common/cursors/cursor.js';
import {CursorRange} from '/common/cursors/range.js';
import {EventHandler} from '/common/event_handler.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Command} from '../common/command.js';
import {TtsSpeechProperties} from '../common/tts_types.js';

import {ChromeVoxRange} from './chromevox_range.js';
import {CommandHandlerInterface} from './input/command_handler_interface.js';

// setTimeout and its clean-up are referencing each other. So, we need to set
// "ignoreReadBeforeAssign" in this file. ESLint doesn't support per-line rule
// option modification.
/* eslint prefer-const: ["error", {"ignoreReadBeforeAssign": true}] */

type AutomationNode = chrome.automation.AutomationNode;
import Dir = constants.Dir;
import EventType = chrome.automation.EventType;
type Unary = AutomationPredicate.Unary;
const RoleType = chrome.automation.RoleType;

/**
 * Handles scrolling, based either on a user command or an event firing.
 * Most of the logic is to support ARC++.
 */
export class AutoScrollHandler {
  private isScrolling_ = false;
  private scrollingNode_: AutomationNode | null = null;
  private rangeBeforeScroll_: CursorRange | null = null;
  private lastScrolledTime_ = new Date(0);
  private relatedFocusEventHappened_ = false;
  private allowWebContentsForTesting_ = false;

  static instance: AutoScrollHandler;

  static init(): void {
    AutoScrollHandler.instance = new AutoScrollHandler();
  }

  /**
   * This should be called before any command triggers ChromeVox navigation.
   *
   * @param target The range that is going to be navigated
   *     before scrolling.
   * @param dir The direction to navigate.
   * @param pred The predicate to match.
   * @param unit The unit to navigate by.
   * @param speechProps The optional speech properties
   *     given to |navigateTo| to provide feedback from the current command.
   * @param rootPred The predicate that expresses
   *     the current navigation root.
   * @param retryCommandFunc The callback used to retry the command
   *     with refreshed tree after scrolling.
   * @return True if the given navigation can be executed. False if the given
   *     navigation shouldn't happen, and AutoScrollHandler handles the command
   *     instead.
   */
  onCommandNavigation(
      target: CursorRange, dir: Dir, pred: Unary | null,
      unit: CursorUnit | null, speechProps: TtsSpeechProperties | null,
      rootPred: Unary, retryCommandFunc: Function): boolean {
    if (this.isScrolling_) {
      // Prevent interrupting the current scrolling.
      return false;
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (!target.start?.node || !ChromeVoxRange.current!.start.node) {
      return true;
    }

    const rangeBeforeScroll = ChromeVoxRange.current;
    let scrollable: AutomationNode | null | undefined =
        this.findScrollableAncestor_(target);

    // At the beginning or the end of the document, there is a case where the
    // range stays there. It's worth trying scrolling the containing scrollable.
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (!scrollable && target.equals(rangeBeforeScroll!) &&
        (unit === CursorUnit.WORD || unit === CursorUnit.CHARACTER)) {
      scrollable =
          this.tryFindingContainingScrollableIfAtEdge_(
              target, dir, scrollable ?? undefined);
    }

    if (!scrollable) {
      return true;
    }

    this.isScrolling_ = true;
    this.scrollingNode_ = scrollable;
    this.rangeBeforeScroll_ = rangeBeforeScroll;
    this.lastScrolledTime_ = new Date();
    this.relatedFocusEventHappened_ = false;

    this.scrollForCommandNavigation_(
            target, dir, pred, unit, speechProps ?? undefined, rootPred,
            retryCommandFunc)
        .catch(() => this.isScrolling_ = false);
    return false;
  }

  /**
   * This function will scroll to find nodes that are offscreen and not in the
   * tree.
   * @param bound The current cell node.
   * @param command the command handler command.
   * @param dir the direction of movement
   * @return True if the given navigation can be executed. False if
   *     the given navigation shouldn't happen, and AutoScrollHandler handles
   *     the command instead.
   */
  scrollToFindNodes(
      bound: AutomationNode, command: Command, target: CursorRange, dir: Dir,
      postScrollCallback: Function): boolean {
    if (bound.parent?.hasHiddenOffscreenNodes && target) {
      let pred = null;
      // Handle grids going over edge.

      if (bound.parent?.role === RoleType.GRID) {
        // TODO(b/314203187): Not null asserted, check that this is correct.
        const currentRow = bound.tableCellRowIndex;
        const totalRows = bound.parent!.tableRowCount!;
        const currentCol = bound.tableCellColumnIndex;
        const totalCols = bound.parent!.tableColumnCount!;
        if (command === Command.NEXT_ROW || command === Command.PREVIOUS_ROW) {
          if (dir === Dir.BACKWARD && currentRow === 0) {
            return true;
          } else if (
              dir === Dir.FORWARD && currentRow === (totalRows - 1)) {
            return true;
          }
          // Create predicate
          pred = AutomationPredicate.makeTableCellPredicate(bound, {
            row: true,
            dir,
          });
        } else if (
            command === Command.NEXT_COL || command === Command.PREVIOUS_COL) {
          if (dir === Dir.BACKWARD && currentCol === 0) {
            return true;
          } else if (
              dir === Dir.FORWARD && currentCol === (totalCols - 1)) {
            return true;
          }
          // Create predicate
          pred = AutomationPredicate.makeTableCellPredicate(bound, {
            col: true,
            dir,
          });
        }
      }

      return this.onCommandNavigation(
          target, dir, pred, null, null, AutomationPredicate.root,
          postScrollCallback);
    }
    return true;
  }

  /** @param target The range that is going to be navigated before scrolling. */
  private findScrollableAncestor_(target: CursorRange): AutomationNode | null {
    let ancestors;
    if (ChromeVoxRange.current && target.equals(ChromeVoxRange.current)) {
      ancestors = AutomationUtil.getAncestors(target.start.node);
    } else {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      ancestors = AutomationUtil.getUniqueAncestors(
          target.start.node, ChromeVoxRange.current!.start.node);
    }
    // Check if we are in ARC++. Scrolling behavior should only happen there,
    // where additional nodes are not loaded until the user scrolls.
    if (!this.allowWebContentsForTesting_ &&
        !ancestors.find(
            node => node.role === RoleType.APPLICATION)) {
      return null;
    }
    const scrollable =
        ancestors.find(node => AutomationPredicate.autoScrollable(node));
    return scrollable ?? null;
  }

  private tryFindingContainingScrollableIfAtEdge_(
      target: CursorRange, dir: Dir, scrollable: AutomationNode | undefined):
      AutomationNode | undefined {
    let current = target.start.node;
    let parent = current.parent;
    while (parent?.root === current.root) {
      if (!(dir === Dir.BACKWARD && parent?.firstChild === current) &&
          !(dir === Dir.FORWARD && parent?.lastChild === current)) {
        // Currently on non-edge node. Don't try scrolling.
        return undefined;
      }
      if (AutomationPredicate.autoScrollable(current)) {
        scrollable = current;
      }
      // TODO(b/314203187): Not null asserted, check that this is correct.
      current = parent!;
      parent = current.parent;
    }
    return scrollable;
  }

  /**
   * @param target The range that is going to be navigated before scrolling.
   * @param dir The direction to navigate.
   * @param pred The predicate to match.
   * @param unit The unit to navigate by.
   * @param speechProps The optional speech properties given to |navigateTo| to
   *     provide feedback for the current command.
   * @param rootPred The predicate that expresses the current navigation root.
   * @param retryCommandFunc The callback used to retry the command with
   *     refreshed tree after scrolling.
   */
  private async scrollForCommandNavigation_(
      target: CursorRange, dir: Dir, pred: Unary | null,
      unit: CursorUnit | null, speechProps: TtsSpeechProperties | undefined,
      rootPred: Unary, retryCommandFunc: Function): Promise<void> {
    const scrollResult =
        await this.scrollInDirection_(this.scrollingNode_ ?? undefined, dir);
    if (!scrollResult) {
      this.isScrolling_ = false;
      ChromeVoxRange.navigateTo(target, false, speechProps);
      return;
    }

    // Wait for a scrolled event or timeout.
    await this.waitForScrollEvent_(this.scrollingNode_ ?? undefined);

    // When a scrolling animation happens, SCROLL_POSITION_CHANGED event can
    // be dispatched multiple times, and there is a chance that the ui tree is
    // in an intermediate state. Just wait for a while so that the UI gets
    // stabilized.
    await new Promise(resolve => setTimeout(resolve, DELAY_HANDLE_SCROLLED_MS));

    this.isScrolling_ = false;

    if (!this.scrollingNode_) {
      throw Error('Illegal state in AutoScrollHandler.');
    }
    if (!this.scrollingNode_.root) {
      // Maybe scrollable node has disappeared. Do nothing.
      return;
    }

    if (this.relatedFocusEventHappened_) {
      const nextRange = this.handleScrollingInAndroidRecyclerView_(
          pred, unit, dir, rootPred, this.scrollingNode_);

      ChromeVoxRange.navigateTo(nextRange ?? target, false, speechProps);
      return;
    }

    // If the focus has been changed for some reason, do nothing to
    // prevent disturbing the latest navigation.
    // TODO(b/314203187): Not null asserted, check that this is correct.
    if (!ChromeVoxRange.current ||
        !ChromeVoxRange.current.equals(this.rangeBeforeScroll_!)) {
      return;
    }

    // Usual case. Retry navigation with a refreshed tree.
    retryCommandFunc();
  }

  private async scrollInDirection_(
      scrollable: AutomationNode | undefined, dir: Dir): Promise<boolean> {
    return new Promise((resolve, reject) => {
      if (!scrollable) {
        reject('Scrollable cannot be null or undefined when scrolling');
        return;
      }
      if (dir === Dir.FORWARD) {
        scrollable.scrollForward(resolve);
      } else {
        scrollable.scrollBackward(resolve);
      }
    });
  }

  private async waitForScrollEvent_(
      scrollable: AutomationNode | undefined): Promise<void> {
    return new Promise((resolve, reject) => {
      if (!scrollable) {
        reject('Scrollable cannot be null when waiting for scroll event');
        return;
      }
      let cleanUp: VoidFunction;
      let listener: EventHandler;
      let timeoutId: number;
      const onTimeout = (): void => {
        cleanUp();
        reject('timeout to wait for scrolled event.');
      };
      const onScrolled = (): void => {
        cleanUp();
        resolve();
      };
      cleanUp = () => {
        listener.stop();
        clearTimeout(timeoutId);
      };

      // TODO(b/314203187): Not null asserted, check that this is correct.
      listener = new EventHandler(
          scrollable!, RELATED_SCROLL_EVENT_TYPES, onScrolled, {capture: true});
      listener.start();
      timeoutId = setTimeout(onTimeout, TIMEOUT_SCROLLED_EVENT_MS);
    });
  }

  /**
   * This block handles scrolling on Android RecyclerView.
   * When scrolling happens, RecyclerView can delete an invisible item, which
   * might be the focused node before scrolling, or can re-use the focused node
   * for a newly added node. When one of these happens, the focused node first
   * disappears from the ARC tree and the focus is moved up to the View that has
   * the ARC tree. In this case, navigate to the first or the last matching
   * range in the scrollable.
   *
   * @param pred The predicate to match.
   * @param unit The unit to navigate by.
   * @param dir The direction to navigate.
   * @param rootPred The predicate that expresses the current navigation root.
   */
  private handleScrollingInAndroidRecyclerView_(
      pred: Unary | null, unit: CursorUnit | null, dir: Dir, rootPred: Unary,
      scrollable: AutomationNode): CursorRange | null {
    let nextRange = null;
    if (!pred && unit) {
      nextRange = CursorRange.fromNode(scrollable).sync(unit, dir);
      if (unit === CursorUnit.NODE) {
        // TODO(b/314203187): Not null asserted, check that this is correct.
        nextRange = CommandHandlerInterface.instance.skipLabelOrDescriptionFor(
            nextRange!, dir);
      }
    } else if (pred) {
      let node;
      if (dir === Dir.FORWARD) {
        node = AutomationUtil.findNextNode(
            scrollable, dir, pred, {root: rootPred, skipInitialSubtree: false});
      } else {
        // TODO(b/314203187): Not null asserted, check that this is correct.
        node = AutomationUtil.findNodePost(this.scrollingNode_!, dir, pred);
      }
      if (node) {
        nextRange = CursorRange.fromNode(node);
      }
    }
    return nextRange;
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
   * @param node the node that is the focus event target.
   * @return True if others can handle the event. False if the event shouldn't
   *     be propagated.
   */
  onFocusEventNavigation(node: AutomationNode | undefined): boolean {
    if (!this.scrollingNode_ || !node) {
      return true;
    }
    const elapsedTime = new Date().valueOf() - this.lastScrolledTime_.valueOf();
    if (elapsedTime > TIMEOUT_FOCUS_EVENT_DROP_MS) {
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
}

// Variables local to the module.

/**
 * An array of Automation event types that AutoScrollHandler observes when
 * performing a scroll action.
 */
const RELATED_SCROLL_EVENT_TYPES: EventType[] = [
  // This is sent by ARC++.
  EventType.SCROLL_POSITION_CHANGED,
  // These two events are sent by Web and Views via AXEventGenerator.
  EventType.SCROLL_HORIZONTAL_POSITION_CHANGED,
  EventType.SCROLL_VERTICAL_POSITION_CHANGED,
];

/**
 * The timeout that we wait for scroll event since scrolling action's callback
 * is executed.
 * TODO(hirokisato): Find an appropriate timeout in our case. TalkBack uses 500
 * milliseconds. See
 * https://github.com/google/talkback/blob/acd0bc7631a3dfbcf183789c7557596a45319e1f/talkback/src/main/java/ScrollEventInterpreter.java#L169
 */
const TIMEOUT_SCROLLED_EVENT_MS = 1500;

/**
 * The timeout that the focused event should be dropped. This is longer than
 * |TIMEOUT_CALLBACK_MS| because a focus event can happen after the scrolling.
 */
const TIMEOUT_FOCUS_EVENT_DROP_MS = 2000;

/**
 * The delay in milliseconds to wait to handle a scrolled event after the event
 * is first dispatched in order to wait for UI stabilized. See also
 * https://github.com/google/talkback/blob/6c0b475b7f52469e309e51bfcc13de58f18176ff/talkback/src/main/java/com/google/android/accessibility/talkback/interpreters/AutoScrollInterpreter.java#L42
 */
const DELAY_HANDLE_SCROLLED_MS = 150;

TestImportManager.exportForTesting(AutoScrollHandler);
