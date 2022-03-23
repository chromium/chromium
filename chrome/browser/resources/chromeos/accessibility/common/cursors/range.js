// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Structures related to ranges, which are pairs of cursors over
 * the automation tree.
 */

goog.provide('cursors.Range');

goog.require('AutomationUtil');
goog.require('constants');
goog.require('cursors.Cursor');

goog.scope(function() {
const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;


/**
 * Represents a range in the automation tree. There is no visible selection on
 * the page caused by usage of this object.
 * It is assumed that the caller provides |start| and |end| in document order.
 */
cursors.Range = class {
  /**
   * @param {!cursors.Cursor} start
   * @param {!cursors.Cursor} end
   */
  constructor(start, end) {
    /** @type {!cursors.Cursor} @private */
    this.start_ = start;
    /** @type {!cursors.Cursor} @private */
    this.end_ = end;
  }

  /**
   * Convenience method to construct a Range surrounding one node.
   * @param {!AutomationNode} node
   * @return {!cursors.Range}
   */
  static fromNode(node) {
    const cursor = cursors.WrappingCursor.fromNode(node);
    return new cursors.Range(cursor, cursor);
  }

  /**
   * Given |rangeA| and |rangeB| in order, determine which |Dir|
   * relates them.
   * @param {!cursors.Range} rangeA
   * @param {!cursors.Range} rangeB
   * @return {Dir}
   */
  static getDirection(rangeA, rangeB) {
    if (!rangeA || !rangeB) {
      return Dir.FORWARD;
    }

    if (!rangeA.start.node || !rangeA.end.node || !rangeB.start.node ||
        !rangeB.end.node) {
      return Dir.FORWARD;
    }

    // They are the same range.
    if (rangeA.start.node === rangeB.start.node &&
        rangeB.end.node === rangeA.end.node) {
      return Dir.FORWARD;
    }

    const testDirA =
        AutomationUtil.getDirection(rangeA.start.node, rangeB.end.node);
    const testDirB =
        AutomationUtil.getDirection(rangeB.start.node, rangeA.end.node);

    // The two ranges are either partly overlapping or non overlapping.
    if (testDirA === Dir.FORWARD && testDirB === Dir.BACKWARD) {
      return Dir.FORWARD;
    } else if (testDirA === Dir.BACKWARD && testDirB === Dir.FORWARD) {
      return Dir.BACKWARD;
    } else {
      return testDirA;
    }
  }

  /**
   * Returns true if |rhs| is equal to this range.
   * Use this for strict equality between ranges.
   * @param {!cursors.Range} rhs
   * @return {boolean}
   */
  equals(rhs) {
    return this.start_.equals(rhs.start) && this.end_.equals(rhs.end);
  }


  /**
   * Similar to above equals(), but does not trigger recovery in either start or
   * end cursor. Use this for strict equality between ranges.
   * @param {!cursors.Range} rhs
   * @return {boolean}
   */
  equalsWithoutRecovery(rhs) {
    return this.start_.equalsWithoutRecovery(rhs.start) &&
        this.end_.equalsWithoutRecovery(rhs.end);
  }

  /**
   * Returns true if |rhs| is equal to this range.
   * Use this for loose equality between ranges.
   * @param {!cursors.Range} rhs
   * @return {boolean}
   */
  contentEquals(rhs) {
    return this.start_.contentEquals(rhs.start) &&
        this.end_.contentEquals(rhs.end);
  }

  /**
   * Gets the directed end cursor of this range.
   * @param {Dir} dir Which endpoint cursor to return;
   *     Dir.FORWARD for end,
   * Dir.BACKWARD for start.
   * @return {!cursors.Cursor}
   */
  getBound(dir) {
    return dir === Dir.FORWARD ? this.end_ : this.start_;
  }

  /**
   * Returns true if either start or end of this range requires recovery.
   * @return {boolean}
   */
  requiresRecovery() {
    return this.start_.requiresRecovery() || this.end_.requiresRecovery();
  }

  /**
   * @return {!cursors.Cursor}
   */
  get start() {
    return this.start_;
  }

  /**
   * @return {!cursors.Cursor}
   */
  get end() {
    return this.end_;
  }

  /**
   * Returns true if this range covers less than a node.
   * @return {boolean}
   */
  isSubNode() {
    const startIndex = this.start.index;
    const endIndex = this.end.index;
    return this.start.node === this.end.node && startIndex !== -1 &&
        endIndex !== -1 && startIndex !== endIndex &&
        (startIndex !== 0 || endIndex !== this.start.getText().length);
  }

  /**
   * Returns true if this range covers inline text (i.e. each end points to an
   * inlineTextBox).
   * @return {boolean?}
   */
  isInlineText() {
    return this.start.node && this.end.node &&
        this.start.node.role === this.end.node.role &&
        this.start.node.role === RoleType.INLINE_TEXT_BOX;
  }

  /**
   * Makes a Range which has been moved from this range by the given unit and
   * direction.
   * @param {cursors.Unit} unit
   * @param {Dir} dir
   * @return {cursors.Range}
   */
  move(unit, dir) {
    let newStart = this.start_;
    if (!newStart.node) {
      return this;
    }

    let newEnd;
    switch (unit) {
      case cursors.Unit.CHARACTER:
        newStart = newStart.move(unit, cursors.Movement.DIRECTIONAL, dir);
        newEnd = newStart.move(unit, cursors.Movement.DIRECTIONAL, Dir.FORWARD);
        // Character crossed a node; collapses to the end of the node.
        if (newStart.node !== newEnd.node) {
          newEnd = new cursors.Cursor(newStart.node, newStart.index + 1);
        }
        break;
      case cursors.Unit.WORD:
      case cursors.Unit.LINE:
        newStart = newStart.move(unit, cursors.Movement.DIRECTIONAL, dir);
        newStart = newStart.move(unit, cursors.Movement.BOUND, Dir.BACKWARD);
        newEnd = newStart.move(unit, cursors.Movement.BOUND, Dir.FORWARD);
        break;
      case cursors.Unit.NODE:
        newStart = newStart.move(unit, cursors.Movement.DIRECTIONAL, dir);
        newEnd = newStart;
        break;
      default:
        throw Error('Invalid unit: ' + unit);
    }
    return new cursors.Range(newStart, newEnd);
  }

  /**
   * Select the text contained within this range.
   */
  select() {
    let start = this.start_, end = this.end_;
    if (this.start.compare(this.end) === Dir.BACKWARD) {
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
      let endIndex = end.index === cursors.NODE_INDEX ? end.selectionIndex + 1 :
                                                        end.selectionIndex;

      // If the range covers more than one node, ends on the node, and is over
      // text, then adjust the selection to cover the entire end node.
      if (start.node !== end.node && end.index === cursors.NODE_INDEX &&
          AutomationPredicate.text(end.node)) {
        endIndex = end.getText().length;
      }

      // Richly editables should always set a caret, but not select. This
      // makes it possible to navigate through content editables using
      // ChromeVox keys and not hear selections as you go.
      if (startNode.state[StateType.RICHLY_EDITABLE] ||
          endNode.state[StateType.RICHLY_EDITABLE]) {
        endIndex = startIndex;
      }

      chrome.automation.setDocumentSelection({
        anchorObject: startNode,
        anchorOffset: startIndex,
        focusObject: endNode,
        focusOffset: endIndex
      });
    }
  }

  /**
   * Returns a new range that matches to the given unit and direction in the
   * current range. If no matching range is found, then null is returned.
   * Note that there is a chance that new range's end spans beyond the current
   * end when the given unit is larger than the current range.
   * @param {cursors.Unit} unit
   * @param {Dir} dir
   * @return {cursors.Range}
   */
  sync(unit, dir) {
    switch (unit) {
      case cursors.Unit.CHARACTER:
      case cursors.Unit.WORD:
        let startCursor = this.start;
        if (!AutomationPredicate.leafWithWordStop(startCursor.node)) {
          let startNode = startCursor.node;
          if (dir === Dir.FORWARD) {
            startNode = AutomationUtil.findNextNode(
                startNode, Dir.FORWARD, AutomationPredicate.leafWithWordStop,
                {skipInitialSubtree: false});
          } else {
            startNode = AutomationUtil.findNodePost(
                startNode, dir, AutomationPredicate.leafWithWordStop);
          }
          if (!startNode) {
            return null;
          }
          startCursor = cursors.WrappingCursor.fromNode(startNode);
        }

        const start = startCursor.move(unit, cursors.Movement.SYNC, dir);
        if (!start) {
          return null;
        }
        let end = start.move(unit, cursors.Movement.BOUND, Dir.FORWARD);
        if (start.node !== end.node || start.equals(end)) {
          // Character crossed a node or reached the end.
          // Collapses to the end of the node.
          end = new cursors.WrappingCursor(start.node, start.getText().length);
        }
        return new cursors.Range(start, end);
      case cursors.Unit.LINE:
        let newNode;
        if (dir === Dir.FORWARD) {
          newNode = AutomationUtil.findNodeUntil(
              this.start.node, dir, AutomationPredicate.linebreak);
        } else {
          newNode = AutomationUtil.findLastNode(
              this.start.node, AutomationPredicate.leaf);
        }
        if (!newNode) {
          return null;
        }
        return cursors.Range.fromNode(newNode);
      case cursors.Unit.TEXT:
      case cursors.Unit.NODE:
        const pred = unit === cursors.Unit.TEXT ? AutomationPredicate.leaf :
                                                  AutomationPredicate.object;
        let node;
        if (dir === Dir.FORWARD) {
          node = AutomationUtil.findNextNode(
              this.start.node, Dir.FORWARD, pred, {skipInitialSubtree: false});
        } else {
          node = AutomationUtil.findNodePost(this.start.node, dir, pred);
        }
        if (!node) {
          return null;
        }

        return cursors.Range.fromNode(node);
      default:
        throw Error('Invalid unit: ' + unit);
    }
  }

  /**
   * Returns true if this range has either cursor end on web content.
   * @return {boolean}
   */
  isWebRange() {
    return this.isValid() &&
        (this.start.node.root.role !== RoleType.DESKTOP ||
         this.end.node.root.role !== RoleType.DESKTOP);
  }

  /**
   * Returns whether this range has valid start and end cursors.
   * @return {boolean}
   */
  isValid() {
    return this.start.isValid() && this.end.isValid();
  }

  /**
   * Compares this range with |rhs|.
   * @param {cursors.Range} rhs
   * @return {Dir|undefined} Dir.BACKWARD if |rhs| comes
   *     before this range in
   * document order. Dir.FORWARD if |rhs| comes after this range.
   * Undefined otherwise.
   */
  compare(rhs) {
    const startDir = this.start.compare(rhs.start);
    const endDir = this.end.compare(rhs.end);
    if (startDir !== endDir) {
      return undefined;
    }

    return startDir;
  }

  /**
   * Returns an undirected version of this range.
   * @return {!cursors.Range}
   */
  normalize() {
    if (this.start.compare(this.end) === Dir.BACKWARD) {
      return new cursors.Range(this.end, this.start);
    }
    return this;
  }

  /**
   * Returns true if this range was created after wrapping. For example, moving
   * from a range at the end of a web contents to [this] range at the beginning
   * of the document.
   * @return {boolean}
   */
  get wrapped() {
    return this.start_.wrapped || this.end_.wrapped;
  }
};
});  // goog.scope
