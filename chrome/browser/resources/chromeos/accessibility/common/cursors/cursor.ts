// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Structures related to cursors that point to and select parts of
 * the automation tree.
 */

import {AutomationPredicate} from '../automation_predicate.js';
import {AutomationUtil} from '../automation_util.js';
import {constants} from '../constants.js';
import {StringUtil} from '../string_util.js';
import {TestImportManager} from '../testing/test_import_manager.js';
import {AutomationTreeWalker} from '../tree_walker.js';

import {AncestryRecoveryStrategy, RecoveryStrategy} from './recovery_strategy.js';

import AutomationNode = chrome.automation.AutomationNode;
import Dir = constants.Dir;
import RoleType = chrome.automation.RoleType;
import StateType = chrome.automation.StateType;

/**
 * The special index that represents a cursor pointing to a node without
 * pointing to any part of its accessible text.
 */
export const CURSOR_NODE_INDEX = -1;

/** Represents units of CursorMovement. */
export enum CursorUnit {
  /** A single character within accessible name or value. */
  CHARACTER = 'character',

  /** A range of characters (given by attributes on automation nodes). */
  WORD = 'word',

  /** A text node. */
  TEXT = 'text',

  /** A leaf node. */
  NODE = 'node',

  /** A leaf node for guesture navigation. */
  GESTURE_NODE = 'gesture_node',

  /**
   * A node or in line textbox that immediately precedes or follows a visual
   *     line break.
   */
  LINE = 'line',
}

/** Represents the ways in which cursors can move given a cursor unit. */
export enum CursorMovement {
  /** Move to the beginning or end of the current unit. */
  BOUND = 'bound',

  /** Move to the next unit in a particular direction. */
  DIRECTIONAL = 'directional',

  /**
   * Move to the beginning or end of the current cursor. Only supports
   * Unit.CHARACTER and Unit.WORD
   */
  SYNC = 'sync',
}

/**
 * Represents a position within the automation tree.
 */
export class Cursor {
  private index_: number;
  private recovery_: RecoveryStrategy;
  private wrapped_: boolean;

  /**
   * @param index A 0-based index into this cursor node's primary
   * accessible name. An index of |CURSOR_NODE_INDEX| means the node as a
   * whole is pointed to and covers the case where the accessible text is
   * empty.
   * @param args
   *     wrapped: determines whether this cursor wraps when moved beyond a
   * document boundary.
   *     preferNodeStartEquivalent: When true,moves this cursor to the start of
   * the next node when it points to the end of the current node.
   */
  constructor(node: AutomationNode, index: number, args: {
    wrapped?: boolean,
    preferNodeStartEquivalent?: boolean,
  } = {}) {
    // Compensate for specific issues in Blink.
    // TODO(dtseng): Pass through affinity; if upstream, skip below.
    // TODO(b/314203187): Not null asserted, check these to make sure this is
    // correct.
    if (node.role === RoleType.STATIC_TEXT && node.name!.length === index &&
        !args.preferNodeStartEquivalent) {
      // Re-interpret this case as the beginning of the next node.
      const nextNode = AutomationUtil.findNextNode(
          node, Dir.FORWARD, AutomationPredicate.leafOrStaticText,
          {root: r => r === node.root});

      // The exception is when a user types at the end of a line. In that
      // case, staying on the current node is appropriate.
      if (node && node.nextOnLine && node.nextOnLine.role && nextNode) {
        node = nextNode;
        index = 0;
      }
    }

    this.index_ = index;
    this.recovery_ = new AncestryRecoveryStrategy(node);
    this.wrapped_ = args.wrapped || false;
  }

  /** Convenience method to construct a Cursor from a node. */
  static fromNode(node: AutomationNode): Cursor {
    return new Cursor(node, CURSOR_NODE_INDEX);
  }

  /**
   * Gives a function that returns true for leaf types for |unit| navigations.
   */
  static getLeafPredForUnit(unit: CursorUnit): AutomationPredicate.Unary {
    switch (unit) {
      case CursorUnit.TEXT:
        return AutomationPredicate.leaf;
      case CursorUnit.GESTURE_NODE:
        return AutomationPredicate.gestureObject;
      default:
        return AutomationPredicate.object;
    }
  }

  /**
   * Returns true if |rhs| is equal to this cursor.
   * Use this for strict equality between cursors.
   */
  equals(rhs: Cursor): boolean {
    return this.node === rhs.node && this.index === rhs.index;
  }

  equalsWithoutRecovery(rhs: Cursor): boolean {
    return this.recovery_.equalsWithoutRecovery(rhs.recovery_);
  }

  /**
   * Returns true if |rhs| is equal to this cursor.
   * Use this for loose equality between cursors where specific
   * character-based indicies do not matter such as when processing
   * node-targeted events.
   */
  contentEquals(rhs: Cursor): boolean {
    // First, normalize the two nodes so they both point to the first non-text
    // node.
    let lNode: AutomationNode|undefined = this.node;
    let rNode: AutomationNode|undefined = rhs.node;
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
   * @return Dir.BACKWARD if |rhs| comes before this cursor in
   * document order. Forward otherwise.
   */
  compare(rhs: Cursor): Dir {
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
   */
  get node(): AutomationNode {
    if (this.requiresRecovery()) {
      // If we need to recover, the index is no longer valid.
      this.index_ = CURSOR_NODE_INDEX;
    }
    return this.recovery_.node;
  }

  get index(): number {
    return this.index_;
  }

  /** A node appropriate for making selections. */
  get selectionNode(): AutomationNode {
    // TODO(accessibility): figure out if we still need the above property.
    return this.node;
  }

  /**
   * An index appropriate for making selections.  If this cursor has a
   * CURSOR_NODE_INDEX index, the selection index is a node offset e.g. the
   * index in parent. If not, the index is a character offset.
   */
  get selectionIndex(): number {
    return this.index_ === CURSOR_NODE_INDEX ? 0 : this.index_;
  }

  /** Gets the accessible text of the node associated with this cursor. */
  getText(): string {
    return AutomationUtil.getText(this.node);
  }

  /**
   * Makes a Cursor which has been moved from this cursor by the unit in the
   * given direction using the given movement type.
   * @return The moved cursor.
   */
  move(unit: CursorUnit, movement: CursorMovement, dir: constants.Dir): Cursor {
    const originalNode = this.node;
    if (!originalNode) {
      return this;
    }

    let newNode: AutomationNode|null = originalNode;
    let newIndex = this.index_;

    switch (unit) {
      case CursorUnit.CHARACTER:
        const text = this.getText();

        switch (movement) {
          case CursorMovement.BOUND:
          case CursorMovement.DIRECTIONAL:
            if (newIndex === CURSOR_NODE_INDEX) {
              newIndex = 0;
            }

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
          case CursorMovement.SYNC:
            if (newIndex === CURSOR_NODE_INDEX) {
              newIndex = dir === Dir.FORWARD ?
                  0 :
                  StringUtil.previousCodePointOffset(text, text.length);
            } else {
              newIndex = dir === Dir.FORWARD ?
                  StringUtil.nextCodePointOffset(text, newIndex) :
                  StringUtil.previousCodePointOffset(text, newIndex);
            }
            if (newIndex < 0 || newIndex >= text.length) {
              // unfortunate case. Fallback to return the same one as |this|.
              newIndex = this.index_;
            }
            break;
        }
        break;
      case CursorUnit.WORD:
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
        if (newIndex < firstWordStart && movement !== CursorMovement.SYNC) {
          // Also catches CURSOR_NODE_INDEX case.
          newIndex = firstWordStart;
        }

        switch (movement) {
          case CursorMovement.BOUND: {
            let wordStarts;
            let wordEnds;
            if (newNode.role === RoleType.INLINE_TEXT_BOX) {
              wordStarts = newNode.wordStarts;
              wordEnds = newNode.wordEnds;
            } else {
              wordStarts = newNode.nonInlineTextWordStarts;
              wordEnds = newNode.nonInlineTextWordEnds;
            }
            let start;
            let end;
            // TODO(b/314203187): Not nulls asserted, check these to make sure
            // they are correct.
            for (let i = 0; i < wordStarts!.length; i++) {
              if (newIndex >= wordStarts![i] && newIndex < wordEnds![i]) {
                start = wordStarts![i];
                end = wordEnds![i];
                break;
              }
            }
            if (start !== undefined && end !== undefined) {
              newIndex = dir === Dir.FORWARD ? end : start;
            }
          } break;
          // @ts-expect-error Fallthrough case in switch
          case CursorMovement.SYNC:
            if (newIndex === CURSOR_NODE_INDEX) {
              newIndex = dir === Dir.FORWARD ? firstWordStart - 1 :
                                               this.getText().length;
            }
          // fallthrough
          case CursorMovement.DIRECTIONAL: {
            let wordStarts;
            let wordEnds;
            let start;
            if (newNode.role === RoleType.INLINE_TEXT_BOX) {
              wordStarts = newNode.wordStarts;
              wordEnds = newNode.wordEnds;
            } else {
              wordStarts = newNode.nonInlineTextWordStarts;
              wordEnds = newNode.nonInlineTextWordEnds;
            }
            // Go to the next word stop in the same piece of text.
            // TODO(b/314203187): Not nulls asserted, check these to make sure
            // they are correct.
            for (let i = 0; i < wordStarts!.length; i++) {
              if ((dir === Dir.FORWARD && newIndex < wordStarts![i]) ||
                  (dir === Dir.BACKWARD && newIndex >= wordEnds![i])) {
                start = wordStarts![i];
                if (dir === Dir.FORWARD) {
                  break;
                }
              }
            }
            if (start !== undefined) {
              // Successfully found the next word stop within the same text
              // node.
              newIndex = start;
            } else if (movement === CursorMovement.DIRECTIONAL) {
              // Use adjacent word in adjacent next node in direction |dir|.
              if (dir === Dir.BACKWARD && newIndex > firstWordStart) {
                // The backward case is special at the beginning of nodes.
                newIndex = firstWordStart;
              } else {
                // TODO(b/314203187): Not null asserted, check these to make
                // sure this is correct.
                newNode = AutomationUtil.findNextNode(
                    newNode, dir, AutomationPredicate.leafWithWordStop,
                    {root: r => r === newNode!.root});
                if (newNode) {
                  let starts;
                  if (newNode.role === RoleType.INLINE_TEXT_BOX) {
                    starts = newNode.wordStarts;
                  } else {
                    starts = newNode.nonInlineTextWordStarts;
                  }
                  // TODO(b/314203187): Not nulls asserted, check these to make
                  // sure they are correct.
                  if (starts!.length) {
                    newIndex = dir === Dir.BACKWARD ?
                        starts![starts!.length - 1] :
                        starts![0];
                  }
                }
              }
            }
          }
        }
        break;
      case CursorUnit.TEXT:
      case CursorUnit.NODE:
      case CursorUnit.GESTURE_NODE:
        switch (movement) {
          case CursorMovement.BOUND:
            newIndex = dir === Dir.FORWARD ? this.getText().length - 1 : 0;
            break;
          case CursorMovement.DIRECTIONAL:
            const pred = Cursor.getLeafPredForUnit(unit);
            newNode =
                AutomationUtil.findNextNode(newNode, dir, pred) || originalNode;
            newIndex = CURSOR_NODE_INDEX;
            break;
        }
        break;
      case CursorUnit.LINE:
        switch (movement) {
          case CursorMovement.BOUND:
            newNode = AutomationUtil.findNodeUntil(
                newNode, dir, AutomationPredicate.linebreak, true);
            newNode = newNode || originalNode;
            newIndex = dir === Dir.FORWARD ?
                AutomationUtil.getText(newNode).length :
                0;
            break;
          case CursorMovement.DIRECTIONAL:
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
    return new Cursor(newNode, newIndex);
  }

  /**
   * Returns the deepest equivalent cursor.
   */
  get deepEquivalent(): Cursor {
    let newNode = this.node;
    let newIndex = this.index_;
    let isTextIndex = false;

    while (newNode.firstChild) {
      // TODO(b/314203187): Not nulls asserted, check these to make sure this is
      // correct.
      if (AutomationPredicate.editText(newNode) &&
          !newNode.state![StateType.MULTILINE]) {
        // Do not reinterpret nodes and indices on this node.
        break;
      } else if (newNode.role === RoleType.STATIC_TEXT) {
        // Text offset.
        // Re-interpret the index as an offset into an inlineTextBox.
        isTextIndex = true;
        let target: AutomationNode|undefined = newNode.firstChild;
        let length = 0;
        while (target && length < newIndex) {
          // TODO(b/314203187): Not nulls asserted, check these to make sure
          // this is correct.
          const newLength = length + target.name!.length;

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
          // TODO(b/314203187): Not nulls asserted, check these to make sure
          // they are correct.
          (!newNode.state![StateType.EDITABLE] ||
           newNode.state![StateType.RICHLY_EDITABLE]) &&
          newIndex <= newNode.children.length) {
        // Valid child node offset. Note that there is a special case where
        // |newIndex === node.children.length|. In these cases, we actually
        // want to position the cursor at the end of the text of
        // |node.children[newIndex - 1]|.
        // |newIndex| is assumed to be > 0.
        if (newIndex === newNode.children.length) {
          // Take the last child.
          // TODO(b/314203187): Not nulls asserted, check these to make sure
          // this is correct.
          newNode = newNode.lastChild!;

          // The |newIndex| is either a text offset or a child offset.
          if (newNode.role === RoleType.STATIC_TEXT) {
            // TODO(b/314203187): Not nulls asserted, check these to make sure
            // this is correct.
            newIndex = newNode.name!.length;
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

        const walker = new AutomationTreeWalker(newNode, Dir.FORWARD, {
          visit: n => !n.firstChild,
          root: n => n === newNode,
        });

        let targetLine;
        let targetIndex = 0;
        let cur = 0;
        while (walker.next().node) {
          // TODO(b/314203187): Not nulls asserted, check these to make sure
          // this is correct.
          const line = walker.node!;
          const lineLength = line.name ? line.name.length : 1;
          cur += lineLength;
          if (cur > newIndex) {
            targetLine = line;
            if (!line.name) {
              targetIndex = CURSOR_NODE_INDEX;
            } else {
              targetIndex = newIndex - (cur - lineLength);
            }
            break;
          }
        }
        if (!targetLine) {
          // If we got here, that means the index is actually beyond the total
          // length of text. Just get the last line.
          targetLine = newNode.lastChild;
          while (targetLine && targetLine.lastChild) {
            targetLine = targetLine.lastChild;
          }
          // TODO(b/314203187): Not nulls asserted, check these to make sure
          // this is correct.
          targetIndex =
              targetLine ? targetLine.name!.length : CURSOR_NODE_INDEX;
        }
        // TODO(b/314203187): Not nulls asserted, check these to make sure this
        // is correct.
        newNode = targetLine!;
        newIndex = targetIndex;
        break;
      }
    }
    if (!isTextIndex) {
      newIndex = CURSOR_NODE_INDEX;
    }

    return new Cursor(newNode, newIndex);
  }

  /** Returns whether this cursor points to a valid position. */
  isValid(): boolean {
    return this.node != null;
  }

  /** Returns true if this cursor requires recovery. */
  requiresRecovery(): boolean {
    return this.recovery_.requiresRecovery();
  }

  /**
   * Returns true if this cursor was created after wrapping. For example, moving
   * from a cursor at the end of a web contents to [this] range at the beginning
   * of the document.
   */
  get wrapped(): boolean {
    return this.wrapped_;
  }
}


/**
 * A Cursor that wraps from beginning to end and vice versa when moved.
 */
export class WrappingCursor extends Cursor {
  /**
   * @param index A 0-based index into this cursor node's primary accessible
   * name. An index of |CURSOR_NODE_INDEX| means the node as a whole is pointed
   * to and covers the case where the accessible text is empty.
   */
  constructor(node: AutomationNode, index: number, args: {
    wrapped?: boolean,
  } = {}) {
    super(node, index, args);
  }

  /** Convenience method to construct a Cursor from a node. */
  static override fromNode(node: AutomationNode): WrappingCursor {
    return new WrappingCursor(node, CURSOR_NODE_INDEX);
  }

  override move(unit: CursorUnit, movement: CursorMovement, dir: constants.Dir):
      Cursor {
    let result: Cursor = this;
    if (!result.node) {
      return this;
    }

    // Regular movement.
    if (!AutomationPredicate.root(this.node) || dir === Dir.FORWARD ||
        movement === CursorMovement.BOUND) {
      result = Cursor.prototype.move.call(this, unit, movement, dir);
    }

    // Moving to the bounds of a unit never wraps.
    if (movement === CursorMovement.BOUND || movement === CursorMovement.SYNC) {
      return new WrappingCursor(result.node, result.index);
    }

    // There are two cases for wrapping:
    // 1. moving forwards from the last element.
    // 2. moving backwards from the first element.
    // Both result in |move| returning the same cursor.
    // For 1, simply place the new cursor on the document node.
    // For 2, place range on the root (if not already there). If at root,
    // try to descend to the first leaf-like object.
    if (movement === CursorMovement.DIRECTIONAL && result.equals(this)) {
      const pred = Cursor.getLeafPredForUnit(unit);
      let endpoint = this.node;
      if (!endpoint) {
        return this;
      }

      // Finds any explicitly provided focus.
      const getDirectedFocus = function(node: AutomationNode): AutomationNode|
          undefined {
            return dir === Dir.FORWARD ? node.nextWindowFocus :
                                         node.previousWindowFocus;
          };

      // Case 1: forwards (find the root-like node).
      let directedFocus;
      while (endpoint.parent) {
        if (directedFocus = getDirectedFocus(endpoint)) {
          break;
        }
        if (AutomationPredicate.root(endpoint)) {
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
        return new WrappingCursor(directedFocus, CURSOR_NODE_INDEX);
      }

      // Always consider this cursor wrapped when moving forward.
      let wrapped = dir === Dir.FORWARD;

      // Case 2: backward (sync downwards to a leaf), if already on the root.
      if (dir === Dir.BACKWARD && endpoint === this.node) {
        wrapped = true;
        endpoint = AutomationUtil.findLastNode(endpoint, pred) || endpoint;
      }

      return new WrappingCursor(endpoint, CURSOR_NODE_INDEX, {wrapped});
    }
    return new WrappingCursor(result.node, result.index);
  }
}

TestImportManager.exportForTesting(Cursor);
