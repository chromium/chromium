// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Structures related to ranges, which are pairs of cursors over
 * the automation tree.
 */

import {AutomationPredicate} from '../automation_predicate.js';
import {AutomationUtil} from '../automation_util.js';
import {constants} from '../constants.js';
import {TestImportManager} from '../testing/test_import_manager.js';

import {Cursor, CURSOR_NODE_INDEX, CursorMovement, CursorUnit, WrappingCursor} from './cursor.js';

type AutomationNode = chrome.automation.AutomationNode;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

/**
 * Represents a range in the automation tree. There is no visible selection on
 * the page caused by usage of this object.
 * It is assumed that the caller provides |start| and |end| in document order.
 */
export class CursorRange {
  constructor(private start_: Cursor, private end_: Cursor) {}

  /** Convenience method to construct a Range surrounding one node. */
  static fromNode(node: AutomationNode): CursorRange {
    const cursor = WrappingCursor.fromNode(node);
    return new CursorRange(cursor, cursor);
  }

  /**
   * Given |rangeA| and |rangeB| in order, determine which |constants.Dir|
   * relates them.
   */
  static getDirection(rangeA: CursorRange, rangeB: CursorRange): constants.Dir {
    if (!rangeA || !rangeB) {
      return constants.Dir.FORWARD;
    }

    if (!rangeA.start.node || !rangeA.end.node || !rangeB.start.node ||
        !rangeB.end.node) {
      return constants.Dir.FORWARD;
    }

    // They are the same range.
    if (rangeA.start.node === rangeB.start.node &&
        rangeB.end.node === rangeA.end.node) {
      return constants.Dir.FORWARD;
    }

    const testDirA =
        AutomationUtil.getDirection(rangeA.start.node, rangeB.end.node);
    const testDirB =
        AutomationUtil.getDirection(rangeB.start.node, rangeA.end.node);

    // The two ranges are either partly overlapping or non overlapping.
    if (testDirA === constants.Dir.FORWARD &&
        testDirB === constants.Dir.BACKWARD) {
      return constants.Dir.FORWARD;
    } else if (
        testDirA === constants.Dir.BACKWARD &&
        testDirB === constants.Dir.FORWARD) {
      return constants.Dir.BACKWARD;
    } else {
      return testDirA;
    }
  }

  /**
   * Returns true if |rhs| is equal to this range.
   * Use this for strict equality between ranges.
   */
  equals(rhs: CursorRange): boolean {
    return this.start_.equals(rhs.start) && this.end_.equals(rhs.end);
  }


  /**
   * Similar to above equals(), but does not trigger recovery in either start or
   * end cursor. Use this for strict equality between ranges.
   */
  equalsWithoutRecovery(rhs: CursorRange): boolean {
    return this.start_.equalsWithoutRecovery(rhs.start) &&
        this.end_.equalsWithoutRecovery(rhs.end);
  }

  /**
   * Returns true if |rhs| is equal to this range.
   * Use this for loose equality between ranges.
   */
  contentEquals(rhs: CursorRange): boolean {
    return this.start_.contentEquals(rhs.start) &&
        this.end_.contentEquals(rhs.end);
  }

  /**
   * Gets the directed end cursor of this range.
   * @param dir Which endpoint cursor to return;
   *     constants.Dir.FORWARD for end,
   * constants.Dir.BACKWARD for start.
   */
  getBound(dir: constants.Dir): Cursor {
    return dir === constants.Dir.FORWARD ? this.end_ : this.start_;
  }

  /**
   * Returns true if either start or end of this range requires recovery.
   */
  requiresRecovery(): boolean {
    return this.start_.requiresRecovery() || this.end_.requiresRecovery();
  }

  get start(): Cursor {
    return this.start_;
  }

  get end(): Cursor {
    return this.end_;
  }

  /** Returns true if this range covers less than a node. */
  isSubNode(): boolean {
    const startIndex = this.start.index;
    const endIndex = this.end.index;
    return this.start.node === this.end.node && startIndex !== -1 &&
        endIndex !== -1 && startIndex !== endIndex &&
        (startIndex !== 0 || endIndex !== this.start.getText().length);
  }

  /**
   * Returns true if this range covers inline text (i.e. each end points to an
   * inlineTextBox).
   */
  isInlineText(): boolean {
    return this.start.node && this.end.node &&
        this.start.node.role === this.end.node.role &&
        this.start.node.role === RoleType.INLINE_TEXT_BOX;
  }

  /**
   * Makes a Range which has been moved from this range by the given unit and
   * direction.
   */
  move(unit: CursorUnit, dir: constants.Dir): CursorRange {
    let newStart = this.start_;
    if (!newStart.node) {
      return this;
    }

    let newEnd;
    switch (unit) {
      case CursorUnit.CHARACTER:
        newStart = newStart.move(unit, CursorMovement.DIRECTIONAL, dir);
        newEnd = newStart.move(
            unit, CursorMovement.DIRECTIONAL, constants.Dir.FORWARD);
        // Character crossed a node; collapses to the end of the node.
        if (newStart.node !== newEnd.node) {
          newEnd = new Cursor(newStart.node, newStart.index + 1);
        }
        break;
      case CursorUnit.WORD:
      case CursorUnit.LINE:
        newStart = newStart.move(unit, CursorMovement.DIRECTIONAL, dir);
        newStart =
            newStart.move(unit, CursorMovement.BOUND, constants.Dir.BACKWARD);
        newEnd =
            newStart.move(unit, CursorMovement.BOUND, constants.Dir.FORWARD);
        break;
      case CursorUnit.NODE:
      case CursorUnit.GESTURE_NODE:
        newStart = newStart.move(unit, CursorMovement.DIRECTIONAL, dir);
        newEnd = newStart;
        break;
      default:
        throw Error('Invalid unit: ' + unit);
    }
    return new CursorRange(newStart, newEnd);
  }

  /** Select the text contained within this range. */
  select(): void {
    let start = this.start_;
    let end = this.end_;
    if (this.start.compare(this.end) === constants.Dir.BACKWARD) {
      start = this.end;
      end = this.start;
    }
    const startNode = start.selectionNode;
    const endNode = end.selectionNode;

    if (!startNode || !endNode) {
      return;
    }

    // Only allow selections within the same web tree.
    if (startNode.root && startNode.root.role === RoleType.ROOT_WEB_AREA &&
        startNode.root === endNode.root) {
      // We want to adjust to select the entire node for node offsets;
      // otherwise, use the plain character offset.
      const startIndex = start.selectionIndex;
      let endIndex = end.index === CURSOR_NODE_INDEX ? end.selectionIndex + 1 :
                                                       end.selectionIndex;

      // If the range covers more than one node, ends on the node, and is over
      // text, then adjust the selection to cover the entire end node.
      if (start.node !== end.node && end.index === CURSOR_NODE_INDEX &&
          AutomationPredicate.text(end.node)) {
        endIndex = end.getText().length;
      }

      // Richly editables should always set a caret, but not select. This
      // makes it possible to navigate through content editables using
      // ChromeVox keys and not hear selections as you go.
      // TODO(b/314203187): Not nulls asserted, check these to make sure they
      // are correct.
      if (startNode.state![StateType.RICHLY_EDITABLE] ||
          endNode.state![StateType.RICHLY_EDITABLE]) {
        endIndex = startIndex;
      }

      chrome.automation.setDocumentSelection({
        anchorObject: startNode,
        anchorOffset: startIndex,
        focusObject: endNode,
        focusOffset: endIndex,
      });
    }
  }

  /**
   * Returns a new range that matches to the given unit and direction in the
   * current range. If no matching range is found, then null is returned.
   * Note that there is a chance that new range's end spans beyond the current
   * end when the given unit is larger than the current range.
   */
  sync(unit: CursorUnit, dir: constants.Dir): CursorRange|null {
    switch (unit) {
      case CursorUnit.CHARACTER:
      case CursorUnit.WORD:
        let startCursor = this.start;
        if (!AutomationPredicate.leafWithWordStop(startCursor.node)) {
          let startNode: AutomationNode|null = startCursor.node;
          // TODO(b/314203187): Not nulls asserted, check these to make sure
          // they are correct.
          if (dir === constants.Dir.FORWARD) {
            startNode = AutomationUtil.findNextNode(
                startNode!, constants.Dir.FORWARD,
                AutomationPredicate.leafWithWordStop,
                {skipInitialSubtree: false});
          } else {
            startNode = AutomationUtil.findNodePost(
                startNode!, dir, AutomationPredicate.leafWithWordStop);
          }
          if (!startNode) {
            return null;
          }
          startCursor = WrappingCursor.fromNode(startNode);
        }

        const start = startCursor.move(unit, CursorMovement.SYNC, dir);
        if (!start) {
          return null;
        }
        let end = start.move(unit, CursorMovement.BOUND, constants.Dir.FORWARD);
        if (start.node !== end.node || start.equals(end)) {
          // Character crossed a node or reached the end.
          // Collapses to the end of the node.
          end = new WrappingCursor(start.node, start.getText().length);
        }
        return new CursorRange(start, end);
      case CursorUnit.LINE:
        let newNode;
        if (dir === constants.Dir.FORWARD) {
          newNode = AutomationUtil.findNodeUntil(
              this.start.node, dir, AutomationPredicate.linebreak);
        } else {
          newNode = AutomationUtil.findLastNode(
              this.start.node, AutomationPredicate.leaf);
        }
        if (!newNode) {
          return null;
        }
        return CursorRange.fromNode(newNode);
      case CursorUnit.TEXT:
      case CursorUnit.NODE:
      case CursorUnit.GESTURE_NODE:
        const pred = Cursor.getLeafPredForUnit(unit);
        let node;
        if (dir === constants.Dir.FORWARD) {
          node = AutomationUtil.findNextNode(
              this.start.node, constants.Dir.FORWARD, pred,
              {skipInitialSubtree: false});
        } else {
          node = AutomationUtil.findNodePost(this.start.node, dir, pred);
        }
        if (!node) {
          return null;
        }

        return CursorRange.fromNode(node);
      default:
        throw Error('Invalid unit: ' + unit);
    }
  }

  /** Returns true if this range has either cursor end on web content. */
  isWebRange(): boolean {
    // TODO(b/314203187): Not nulls asserted, check these to make sure they
    // are correct.
    return this.isValid() &&
        (this.start.node.root!.role !== RoleType.DESKTOP ||
         this.end.node.root!.role !== RoleType.DESKTOP);
  }

  /** Returns whether this range has valid start and end cursors. */
  isValid(): boolean {
    return this.start.isValid() && this.end.isValid();
  }

  /**
   * Compares this range with |rhs|.
   * @return constants.Dir.BACKWARD if |rhs| comes
   *     before this range in
   * document order. constants.Dir.FORWARD if |rhs| comes after this range.
   * Undefined otherwise.
   */
  compare(rhs: CursorRange): constants.Dir|undefined {
    const startDir = this.start.compare(rhs.start);
    const endDir = this.end.compare(rhs.end);
    if (startDir !== endDir) {
      return undefined;
    }

    return startDir;
  }

  /** Returns an undirected version of this range. */
  normalize(): CursorRange {
    if (this.start.compare(this.end) === constants.Dir.BACKWARD) {
      return new CursorRange(this.end, this.start);
    }
    return this;
  }

  /**
   * Returns true if this range was created after wrapping. For example, moving
   * from a range at the end of a web contents to [this] range at the beginning
   * of the document.
   */
  get wrapped(): boolean {
    return this.start_.wrapped || this.end_.wrapped;
  }
}

TestImportManager.exportForTesting(CursorRange);
