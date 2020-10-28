// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Classes related to cursors that point to and select parts of
 * the automation tree.
 */

goog.provide('cursors.Cursor');
goog.provide('cursors.Movement');
goog.provide('cursors.Range');
goog.provide('cursors.Unit');

goog.require('AncestryRecoveryStrategy');
goog.require('AutomationPredicate');
goog.require('AutomationUtil');
goog.require('RecoveryStrategy');
goog.require('StringUtil');
goog.require('constants');

goog.scope(function() {
const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;


/**
 * The special index that represents a cursor pointing to a node without
 * pointing to any part of its accessible text.
 */
cursors.NODE_INDEX = -1;

/**
 * Represents units of cursors.Movement.
 * @enum {string}
 */
cursors.Unit = {
  /** A single character within accessible name or value. */
  CHARACTER: 'character',

  /** A range of characters (given by attributes on automation nodes). */
  WORD: 'word',

  /** A text node. */
  TEXT: 'text',

  /** A leaf node. */
  NODE: 'node',

  /**
   * A node or in line textbox that immediately precedes or follows a visual
   *     line break.
   */
  LINE: 'line'
};

/**
 * Represents the ways in which cursors can move given a cursor unit.
 * @enum {string}
 */
cursors.Movement = {
  /** Move to the beginning or end of the current unit. */
  BOUND: 'bound',

  /** Move to the next unit in a particular direction. */
  DIRECTIONAL: 'directional'
};

/**
 * Represents a position within the automation tree.
 */
cursors.Cursor = class {
  /**
   * @param {!AutomationNode} node
   * @param {number} index A 0-based index into this cursor node's primary
   * accessible name. An index of |cursors.NODE_INDEX| means the node as a
   * whole is pointed to and covers the case where the accessible text is
   * empty.
   */
  constructor(node, index) {
    // Compensate for specific issues in Blink.
    // TODO(dtseng): Pass through affinity; if upstream, skip below.
    if (node.role === RoleType.STATIC_TEXT && node.name.length === index) {
      // Re-interpret this case as the beginning of the next node.
      const nextNode = AutomationUtil.findNextNode(
          node, Dir.FORWARD, AutomationPredicate.leafOrStaticText);

      // The exception is when a user types at the end of a line. In that
      // case, staying on the current node is appropriate.
      if (node && node.nextOnLine && node.nextOnLine.role && nextNode) {
        node = nextNode;
        index = 0;
      }
    }

    /** @type {number} @private */
    this.index_ = index;
    /** @type {RecoveryStrategy} */
    this.recovery_ = new AncestryRecoveryStrategy(node);
  }

  /**
   * Convenience method to construct a Cursor from a node.
   * @param {!AutomationNode} node
   * @return {!cursors.Cursor}
   */
  static fromNode(node) {
    return new cursors.Cursor(node, cursors.NODE_INDEX);
  }

  /**
   * Returns true if |rhs| is equal to this cursor.
   * Use this for strict equality between cursors.
   * @param {!cursors.Cursor} rhs
   * @return {boolean}
   */
  equals(rhs) {
    return this.node === rhs.node && this.index === rhs.index;
  }

  equalsWithoutRecovery(rhs) {
    return this.recovery_.equalsWithoutRecovery(rhs.recovery_);
  }

  /**
   * Returns true if |rhs| is equal to this cursor.
   * Use this for loose equality between cursors where specific
   * character-based indicies do not matter such as when processing
   * node-targeted events.
   * @param {!cursors.Cursor} rhs
   * @return {boolean}
   */
  contentEquals(rhs) {
    // First, normalize the two nodes so they both point to the first non-text
    // node.
    let lNode = this.node;
    let rNode = rhs.node;
    while (lNode &&
           (lNode.role === RoleType.INLINE_TEXT_BOX ||
            lNode.role === RoleType.STATIC_TEXT)) {
      lNode = lNode.parent;
    }
    while (rNode &&
           (rNode.role === RoleType.INLINE_TEXT_BOX ||
            rNode.role === RoleType.STATIC_TEXT)) {
      rNode = rNode.parent;
    }

    // Ignore indicies for now.

    return lNode === rNode && lNode !== undefined;
  }

  /**
   * Compares this cursor with |rhs|.
   * @param {cursors.Cursor} rhs
   * @return Dir.BACKWARD if |rhs| comes before this cursor in
   * document order. Forward otherwise.
   */
  compare(rhs) {
    if (!this.node || !rhs.node) {
      return Dir.FORWARD;
    }

    if (rhs.node === this.node) {
      return rhs.index < this.index ? Dir.BACKWARD : Dir.FORWARD;
    }
    return AutomationUtil.getDirection(this.node, rhs.node);
  }

  /**
   * Returns the node. If the node is invalid since the last time it
   * was accessed, moves the cursor to the nearest valid ancestor first.
   * @return {AutomationNode}
   */
  get node() {
    if (this.recovery_.requiresRecovery()) {
      // If we need to recover, the index is no longer valid.
      this.index_ = cursors.NODE_INDEX;
    }
    return this.recovery_.node;
  }

  /**
   * @return {number}
   */
  get index() {
    return this.index_;
  }

  /**
   * A node appropriate for making selections.
   * @return {AutomationNode}
   * @private
   */
  get selectionNode_() {
    let adjustedNode = this.node;
    if (!adjustedNode) {
      return null;
    }

    // Make no adjustments if we're within non-rich editable content.
    if (adjustedNode.state[StateType.EDITABLE] &&
        !adjustedNode.state[StateType.RICHLY_EDITABLE]) {
      return adjustedNode;
    }

    // Selections over line break nodes are broken.
    const parent = adjustedNode.parent;
    const grandparent = parent && parent.parent;
    if (parent && parent.role === RoleType.LINE_BREAK) {
      adjustedNode = grandparent;
    } else if (grandparent && grandparent.role === RoleType.LINE_BREAK) {
      adjustedNode = grandparent.parent;
    } else if (
        this.index_ === cursors.NODE_INDEX ||
        adjustedNode.role === RoleType.INLINE_TEXT_BOX ||
        adjustedNode.nameFrom !== chrome.automation.NameFromType.CONTENTS) {
      // A node offset or unselectable character offset.
      adjustedNode = parent;
    } else {
      // A character offset into content.
      adjustedNode =
          adjustedNode.find({role: RoleType.STATIC_TEXT}) || adjustedNode;
    }

    return adjustedNode || null;
  }

  /**
   * An index appropriate for making selections.  If this cursor has a
   * cursors.NODE_INDEX index, the selection index is a node offset e.g. the
   * index in parent. If not, the index is a character offset.
   * @return {number}
   * @private
   */
  get selectionIndex_() {
    let adjustedIndex = this.index_;

    if (!this.node) {
      return -1;
    }

    if (this.node.state[StateType.EDITABLE]) {
      if (!this.node.state[StateType.RICHLY_EDITABLE]) {
        return this.index_;
      }
      return this.index_ === cursors.NODE_INDEX ?
          (this.node.indexInParent || 0) :
          this.index_;
    } else if (
        this.node.role === RoleType.INLINE_TEXT_BOX &&
        // Selections under a line break are broken.
        this.node.parent && this.node.parent.role !== RoleType.LINE_BREAK) {
      if (adjustedIndex === cursors.NODE_INDEX) {
        adjustedIndex = 0;
      }

      let sibling = this.node.previousSibling;
      while (sibling) {
        adjustedIndex += sibling.name.length;
        sibling = sibling.previousSibling;
      }
    } else if (
        this.index_ === cursors.NODE_INDEX ||
        this.node.nameFrom !== chrome.automation.NameFromType.CONTENTS) {
      // A node offset or unselectable character offset.

      // The selected node could have been adjusted upwards in the tree.
      let childOfSelection = this.node;
      do {
        adjustedIndex = childOfSelection.indexInParent || 0;
        childOfSelection = childOfSelection.parent;
      } while (childOfSelection && childOfSelection !== this.selectionNode_);
    }
    // A character offset into content is the remaining case. It requires no
    // adjustment.

    return adjustedIndex;
  }

  /**
   * Gets the accessible text of the node associated with this cursor.
   * @return {string}
   */
  getText() {
    return AutomationUtil.getText(this.node);
  }

  /**
   * Makes a Cursor which has been moved from this cursor by the unit in the
   * given direction using the given movement type.
   * @param {cursors.Unit} unit
   * @param {cursors.Movement} movement
   * @param {Dir} dir
   * @return {!cursors.Cursor} The moved cursor.
   */
  move(unit, movement, dir) {
    const originalNode = this.node;
    if (!originalNode) {
      return this;
    }

    let newNode = originalNode;
    let newIndex = this.index_;

    switch (unit) {
      case cursors.Unit.CHARACTER:
        if (newIndex === cursors.NODE_INDEX) {
          newIndex = 0;
        }
        // BOUND and DIRECTIONAL are the same for characters.
        const text = this.getText();
        newIndex = dir === Dir.FORWARD ?
            StringUtil.nextCodePointOffset(text, newIndex) :
            StringUtil.previousCodePointOffset(text, newIndex);
        if (newIndex < 0 || newIndex >= text.length) {
          newNode = AutomationUtil.findNextNode(
              newNode, dir, AutomationPredicate.leafWithText);
          if (newNode) {
            const newText = AutomationUtil.getText(newNode);
            newIndex = dir === Dir.FORWARD ?
                0 :
                StringUtil.previousCodePointOffset(newText, newText.length);
            newIndex = Math.max(newIndex, 0);
          } else {
            newIndex = this.index_;
          }
        }
        break;
      case cursors.Unit.WORD:
        // If we're not already on a node with word stops, find the next one.
        if (!AutomationPredicate.leafWithWordStop(newNode)) {
          newNode =
              AutomationUtil.findNextNode(
                  newNode, Dir.FORWARD, AutomationPredicate.leafWithWordStop,
                  {skipInitialSubtree: false}) ||
              newNode;
        }

        // Ensure start position is on or after first word.
        const firstWordStart =
            (newNode.wordStarts && newNode.wordStarts.length) ?
            newNode.wordStarts[0] :
            0;
        if (newIndex < firstWordStart) {
          // Also catches cursors.NODE_INDEX case.
          newIndex = firstWordStart;
        }

        switch (movement) {
          case cursors.Movement.BOUND: {
            let wordStarts, wordEnds;
            if (newNode.role === RoleType.INLINE_TEXT_BOX) {
              wordStarts = newNode.wordStarts;
              wordEnds = newNode.wordEnds;
            } else {
              wordStarts = newNode.nonInlineTextWordStarts;
              wordEnds = newNode.nonInlineTextWordEnds;
            }
            let start, end;
            for (let i = 0; i < wordStarts.length; i++) {
              if (newIndex >= wordStarts[i] && newIndex < wordEnds[i]) {
                start = wordStarts[i];
                end = wordEnds[i];
                break;
              }
            }
            if (goog.isDef(start) && goog.isDef(end)) {
              newIndex = dir === Dir.FORWARD ? end : start;
            }
          } break;
          case cursors.Movement.DIRECTIONAL: {
            let wordStarts, wordEnds;
            let start;
            if (newNode.role === RoleType.INLINE_TEXT_BOX) {
              wordStarts = newNode.wordStarts;
              wordEnds = newNode.wordEnds;
            } else {
              wordStarts = newNode.nonInlineTextWordStarts;
              wordEnds = newNode.nonInlineTextWordEnds;
            }
            // Go to the next word stop in the same piece of text.
            for (let i = 0; i < wordStarts.length; i++) {
              if ((dir === Dir.FORWARD && newIndex < wordStarts[i]) ||
                  (dir === Dir.BACKWARD && newIndex >= wordEnds[i])) {
                start = wordStarts[i];
                if (dir === Dir.FORWARD) {
                  break;
                }
              }
            }
            if (goog.isDef(start)) {
              // Succesfully found the next word stop within the same text
              // node.
              newIndex = start;
            } else {
              // Use adjacent word in adjacent next node in direction |dir|.
              if (dir === Dir.BACKWARD && newIndex > firstWordStart) {
                // The backward case is special at the beginning of nodes.
                newIndex = firstWordStart;
              } else {
                newNode = AutomationUtil.findNextNode(
                    newNode, dir, AutomationPredicate.leafWithWordStop);
                if (newNode) {
                  let starts;
                  if (newNode.role === RoleType.INLINE_TEXT_BOX) {
                    starts = newNode.wordStarts;
                  } else {
                    starts = newNode.nonInlineTextWordStarts;
                  }
                  if (starts.length) {
                    newIndex = dir === Dir.BACKWARD ?
                        starts[starts.length - 1] :
                        starts[0];
                  }
                }
              }
            }
          }
        }
        break;
      case cursors.Unit.TEXT:
      case cursors.Unit.NODE:
        switch (movement) {
          case cursors.Movement.BOUND:
            newIndex = dir === Dir.FORWARD ? this.getText().length - 1 : 0;
            break;
          case cursors.Movement.DIRECTIONAL:
            const pred = unit === cursors.Unit.TEXT ?
                AutomationPredicate.leaf :
                AutomationPredicate.object;
            newNode =
                AutomationUtil.findNextNode(newNode, dir, pred) || originalNode;
            newIndex = cursors.NODE_INDEX;
            break;
        }
        break;
      case cursors.Unit.LINE:
        switch (movement) {
          case cursors.Movement.BOUND:
            newNode = AutomationUtil.findNodeUntil(
                newNode, dir, AutomationPredicate.linebreak, true);
            newNode = newNode || originalNode;
            newIndex = dir === Dir.FORWARD ?
                AutomationUtil.getText(newNode).length :
                0;
            break;
          case cursors.Movement.DIRECTIONAL:
            newNode = AutomationUtil.findNodeUntil(
                newNode, dir, AutomationPredicate.linebreak);
            if (newNode) {
              newIndex = 0;
            }
            break;
        }
        break;
      default:
        throw Error('Unrecognized unit: ' + unit);
    }
    newNode = newNode || originalNode;
    newIndex = (newIndex !== undefined) ? newIndex : this.index_;
    return new cursors.Cursor(newNode, newIndex);
  }

  /**
   * Returns the deepest equivalent cursor.
   * @return {!cursors.Cursor}
   */
  get deepEquivalent() {
    let newNode = this.node;
    let newIndex = this.index_;
    let isTextIndex = false;
    while (newNode.firstChild) {
      if (newNode.role === RoleType.STATIC_TEXT) {
        // Text offset.
        // Re-interpret the index as an offset into an inlineTextBox.
        isTextIndex = true;
        let target = newNode.firstChild;
        let length = 0;
        while (target && length < newIndex) {
          const newLength = length + target.name.length;

          // Either |newIndex| falls between target's text or |newIndex| is
          // the total length of all sibling text content.
          if ((length <= newIndex && newIndex < newLength) ||
              (newIndex === newLength && !target.nextSibling)) {
            break;
          }
          length = newLength;
          target = target.nextSibling;
        }
        if (target) {
          newNode = target;
          newIndex = newIndex - length;
        }
        break;
      } else if (
          newNode.role !== RoleType.INLINE_TEXT_BOX &&
          // An index inside a content editable or a descendant of a content
          // editable should be treated as a child offset.
          // However, an index inside a simple editable, such as an input
          // element, should be treated as a character offset.
          (!newNode.state[StateType.EDITABLE] ||
           newNode.state[StateType.RICHLY_EDITABLE]) &&
          newIndex <= newNode.children.length) {
        // Valid child node offset. Note that there is a special case where
        // |newIndex == node.children.length|. In these cases, we actually
        // want to position the cursor at the end of the text of
        // |node.children[newIndex - 1]|.
        // |newIndex| is assumed to be > 0.
        if (newIndex === newNode.children.length) {
          // Take the last child.
          newNode = newNode.lastChild;

          // The |newIndex| is either a text offset or a child offset.
          if (newNode.role === RoleType.STATIC_TEXT) {
            newIndex = newNode.name.length;
            isTextIndex = true;
            break;
          }

          // The last valid child index.
          newIndex = newNode.children.length;
        } else {
          newNode = newNode.children[newIndex];
          newIndex = 0;
        }
      } else {
        // This offset is a text offset into the descendant visible
        // text. Approximate this by indexing into the inline text boxes.
        isTextIndex = true;
        const lines = this.getAllLeaves_(newNode);
        if (!lines.length) {
          break;
        }

        let targetLine, targetIndex = 0;
        for (let i = 0, line, cur = 0; line = lines[i]; i++) {
          const lineLength = line.name ? line.name.length : 1;
          cur += lineLength;
          if (cur > newIndex) {
            targetLine = line;
            if (!line.name) {
              targetIndex = cursors.NODE_INDEX;
            } else {
              targetIndex = newIndex - (cur - lineLength);
            }
            break;
          }
        }
        if (!targetLine) {
          // If we got here, that means the index is actually beyond the total
          // length of text. Just get the last line.
          targetLine = lines[lines.length - 1];
          targetIndex =
              targetLine ? targetLine.name.length : cursors.NODE_INDEX;
        }
        newNode = targetLine;
        newIndex = targetIndex;
        break;
      }
    }
    if (!isTextIndex) {
      newIndex = cursors.NODE_INDEX;
    }

    return new this.constructor(newNode, newIndex);
  }

  /**
   * Returns whether this cursor points to a valid position.
   * @return {boolean}
   */
  isValid() {
    return this.node != null;
  }

  /**
   * @private
   * @param {!AutomationNode} node
   * @return {!Array<!AutomationNode>}
   */
  getAllLeaves_(node) {
    let ret = [];
    if (!node.firstChild) {
      ret.push(node);
      return ret;
    }

    for (let i = 0; i < node.children.length; i++) {
      ret = ret.concat(this.getAllLeaves_(node.children[i]));
    }

    return ret;
  }
};


/**
 * A cursors.Cursor that wraps from beginning to end and vice versa when
 * moved.
 */
cursors.WrappingCursor = class extends cursors.Cursor {
  /**
   * @param {!AutomationNode} node
   * @param {number} index A 0-based index into this cursor node's primary
   * accessible name. An index of |cursors.NODE_INDEX| means the node as a
   * whole is pointed to and covers the case where the accessible text is
   * empty.
   */
  constructor(node, index) {
    super(node, index);
  }

  /**
   * Convenience method to construct a Cursor from a node.
   * @param {!AutomationNode} node
   * @return {!cursors.WrappingCursor}
   */
  static fromNode(node) {
    return new cursors.WrappingCursor(node, cursors.NODE_INDEX);
  }

  /** @override */
  move(unit, movement, dir) {
    let result = this;
    if (!result.node) {
      return this;
    }

    // Regular movement.
    if (!AutomationPredicate.root(this.node) || dir === Dir.FORWARD ||
        movement === cursors.Movement.BOUND) {
      result = cursors.Cursor.prototype.move.call(this, unit, movement, dir);
    }

    // Moving to the bounds of a unit never wraps.
    if (movement === cursors.Movement.BOUND) {
      return new cursors.WrappingCursor(result.node, result.index);
    }

    // There are two cases for wrapping:
    // 1. moving forwards from the last element.
    // 2. moving backwards from the first element.
    // Both result in |move| returning the same cursor.
    // For 1, simply place the new cursor on the document node.
    // For 2, place range on the root (if not already there). If at root,
    // try to descend to the first leaf-like object.
    if (movement === cursors.Movement.DIRECTIONAL && result.equals(this)) {
      const pred = unit === cursors.Unit.NODE ? AutomationPredicate.object :
                                                AutomationPredicate.leaf;
      let endpoint = this.node;
      if (!endpoint) {
        return this;
      }

      // Finds any explicitly provided focus.
      const getDirectedFocus = function(node) {
        return dir === Dir.FORWARD ? node.nextFocus : node.previousFocus;
      };

      // Case 1: forwards (find the root-like node).
      let directedFocus;
      while (!AutomationPredicate.root(endpoint) && endpoint.parent) {
        if (directedFocus = getDirectedFocus(endpoint)) {
          break;
        }
        endpoint = endpoint.parent;
      }

      if (directedFocus) {
        directedFocus =
            (dir === Dir.FORWARD ?
                 AutomationUtil.findNodePre(
                     directedFocus, dir, AutomationPredicate.object) :
                 AutomationUtil.findLastNode(directedFocus, pred)) ||
            directedFocus;
        return new cursors.WrappingCursor(directedFocus, cursors.NODE_INDEX);
      }

      // Always play a wrap earcon when moving forward.
      let playEarcon = dir === Dir.FORWARD;

      // Case 2: backward (sync downwards to a leaf), if already on the root.
      if (dir === Dir.BACKWARD && endpoint === this.node) {
        playEarcon = true;
        endpoint = AutomationUtil.findLastNode(endpoint, pred) || endpoint;
      }

      if (playEarcon) {
        ChromeVox.earcons.playEarcon(Earcon.WRAP);
      }

      return new cursors.WrappingCursor(endpoint, cursors.NODE_INDEX);
    }
    return new cursors.WrappingCursor(result.node, result.index);
  }
};


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
    const startNode = start.selectionNode_;
    const endNode = end.selectionNode_;

    if (!startNode || !endNode) {
      return;
    }

    // Only allow selections within the same web tree.
    if (startNode.root && startNode.root.role === RoleType.ROOT_WEB_AREA &&
        startNode.root === endNode.root) {
      // We want to adjust to select the entire node for node offsets;
      // otherwise, use the plain character offset.
      const startIndex = start.selectionIndex_;
      let endIndex = end.index_ === cursors.NODE_INDEX ?
          end.selectionIndex_ + 1 :
          end.selectionIndex_;

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
};
});  // goog.scope
