// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutomationPredicate} from './automation_predicate.js';
import {AutomationUtil} from './automation_util.js';
import {constants} from './constants.js';
import {ParagraphUtils} from './paragraph_utils.js';
import {RectUtil} from './rect_util.js';

import AutomationNode = chrome.automation.AutomationNode;
const PositionType = chrome.automation.PositionType;
const RoleType = chrome.automation.RoleType;

// Utilities for automation nodes in Select-to-Speak.

export class NodeUtils {
  /**
   * Gets the current visibility state for a given node.
   * @param node The starting node.
   */
  static getNodeState(node: AutomationNode): NodeUtils.NodeState {
    if (node === undefined || node.root === null || node.root === undefined) {
      // The node has been removed from the tree, perhaps because the
      // window was closed.
      return NodeUtils.NodeState.NODE_STATE_INVALID;
    }
    // This might not be populated correctly on children nodes even if their
    // parents or roots are now invisible.
    // TODO: Update the C++ bindings to set 'invisible' automatically based
    // on parents, rather than going through parents in JS below.
    // TODO(b/314203187): Determine if not null assertion is appropriate here.
    if (node.state!['invisible']) {
      return NodeUtils.NodeState.NODE_STATE_INVISIBLE;
    }
    // Walk up the tree to make sure the window it is in is not invisible.
    const window = NodeUtils.getNearestContainingWindow(node);
    if (window != null &&
        // TODO(b/314203187): Determine if not null assertion is appropriate
        // here.
        window.state![chrome.automation.StateType.INVISIBLE]) {
      return NodeUtils.NodeState.NODE_STATE_INVISIBLE;
    }
    // TODO: Also need a check for whether the window is minimized,
    // which would also return NodeState.NODE_STATE_INVISIBLE.
    return NodeUtils.NodeState.NODE_STATE_NORMAL;
  }

  /**
   * Returns true if a node should be ignored by Select-to-Speak, or if it
   * is of interest. This does not deal with whether nodes have children --
   * nodes are interesting if they have a name or a value and have an onscreen
   * location.
   * @param node The node to test
   * @param includeOffscreen Whether to include offscreen nodes.
   * @return whether this node should be ignored.
   */
  static shouldIgnoreNode(node: AutomationNode, includeOffscreen: boolean):
      boolean {
    if (NodeUtils.isNodeInvisible(node, includeOffscreen)) {
      return true;
    }
    return ParagraphUtils.isWhitespace(ParagraphUtils.getNodeName(node));
  }

  /**
   * Returns true if the node should be ignored by Select-to-Speak because
   * it was marked with user-select:none. For Inline Text elements, the
   * parent is marked with this attribute, hence the check.
   *
   * @param node The node to test
   * @return whether this node was marked user-select:none
   */
  static isNotSelectable(node: AutomationNode): boolean {
    return Boolean(
        node &&
        (node.notUserSelectableStyle ||
         (node.parent && node.parent.notUserSelectableStyle)));
  }

  /**
   * Returns true if a node is invisible for any reason.
   * @param node The node to test
   * @param includeOffscreen Whether to include offscreen nodes
   *     as visible type nodes.
   * @return whether this node is invisible.
   */
  static isNodeInvisible(node: AutomationNode, includeOffscreen: boolean):
      boolean {
    // TODO(b/314203187): Determine if not null assertion is appropriate here.
    return !node.location || node.state!['invisible'] ||
        (node.state!['offscreen'] && !includeOffscreen);
  }

  /**
   * Gets the first window containing this node.
   * @param node The node to find a window for.
   * @return The node representing the nearest
   *     containing window.
   */
  static getNearestContainingWindow(node: AutomationNode): AutomationNode
      |undefined {
    // Go upwards to root nodes' parents until we find the first window.
    if (node.root && node.root.role === RoleType.ROOT_WEB_AREA) {
      let nextRootParent: AutomationNode|undefined = node;
      // TODO(b/314204374): Check if nextRootParent is undefiend rather than
      // null.
      while (nextRootParent != null &&
             nextRootParent.role !== RoleType.WINDOW &&
             nextRootParent.root != null &&
             nextRootParent.root.role === RoleType.ROOT_WEB_AREA) {
        nextRootParent = nextRootParent.root.parent;
      }
      return nextRootParent;
    }
    // If the parent isn't a root web area, just walk up the tree to find the
    // nearest window.
    let parent: AutomationNode|undefined = node;
    // TODO(b/314204374): Check if parent is undefiend rather than null.
    while (parent != null &&
           parent.role !== chrome.automation.RoleType.WINDOW) {
      parent = parent.parent;
    }
    return parent;
  }

  /**
   * Gets the length of a node's name. Returns 0 if the name is
   * undefined.
   * @param node The node for which to check the name.
   * @return The length of the node's name
   */
  static nameLength(node: AutomationNode): number {
    return node.name ? node.name.length : 0;
  }

  /**
   * Returns true if a node is a text field type, but not for any other type,
   * including contentEditables.
   * @param node The node to check
   * @return True if the node is a text field type.
   */
  static isTextField(node: AutomationNode): boolean {
    return node.role === RoleType.TEXT_FIELD ||
        node.role === RoleType.TEXT_FIELD_WITH_COMBO_BOX;
  }

  /**
   * Gets the first (left-most) leaf node of a node. Returns undefined if
   *  none is found.
   * @param node The node to search for the first leaf.
   * @return The leaf node.
   */
  static getFirstLeafChild(node: AutomationNode): AutomationNode|undefined {
    let result = node.firstChild;
    while (result && result.firstChild) {
      result = result.firstChild;
    }
    return result;
  }

  /**
   * Gets the first (left-most) leaf node of a node. Returns undefined
   * if none is found.
   * @param node The node to search for the first leaf.
   * @return The leaf node.
   */
  static getLastLeafChild(node: AutomationNode): AutomationNode|undefined {
    let result = node.lastChild;
    while (result && result.lastChild) {
      result = result.lastChild;
    }
    return result;
  }

  /**
   * Finds all nodes within the subtree rooted at |node| that overlap
   * a given rectangle.
   * @param node The starting node.
   * @param rect The bounding box to search.
   * @param nodes The matching node array to be populated.
   * @return True if any matches are found.
   */
  static findAllMatching(
      node: AutomationNode,
      rect: {left: number, top: number, width: number, height: number},
      nodes: AutomationNode[]): boolean {
    var found = false;
    for (var c = node.firstChild; c; c = c.nextSibling) {
      if (NodeUtils.findAllMatching(c, rect, nodes)) {
        found = true;
      }
    }

    if (found) {
      return true;
    }

    // Closure needs node.location check here to allow the next few
    // lines to compile.
    if (NodeUtils.shouldIgnoreNode(node, /* don't include offscreen */ false) ||
        node.location === undefined) {
      return false;
    }

    if (RectUtil.overlaps(node.location, rect)) {
      if (!node.children || node.children.length === 0 ||
          node.children[0].role !== RoleType.INLINE_TEXT_BOX) {
        // Only add a node if it has no inlineTextBox children. If
        // it has text children, they will be more precisely bounded
        // and specific, so no need to add the parent node.
        nodes.push(node);
        return true;
      }
    }

    return false;
  }

  /**
   * Finds the deep equivalent node where a selection starts given a node
   * object and selection offset. This is meant to be used in conjunction with
   * the selectionStartObject/selectionStartOffset and
   * selectionEndObject/selectionEndOffset of the automation API.
   * @param parent The parent node of the selection,
   * similar to chrome.automation.selectionStartObject or selectionEndObject.
   * @param offset The integer offset of the selection. This is
   * similar to chrome.automation.selectionStartOffset or selectionEndOffset.
   * @param isStart whether this is the start or end of a selection.
   * @return The node matching the selected offset.
   */
  static getDeepEquivalentForSelection(
      parent: AutomationNode, offset: number,
      isStart: boolean): NodeUtils.Position {
    const automationPosition = parent.createPosition(PositionType.TREE, offset);
    if (!automationPosition.node) {
      // TODO(accessibility): Bugs in AXPosition cause this; for example, a
      // selection on a text field.
      return this.getDeepEquivalentForSelectionDeprecated(
          parent, offset, isStart);
    }

    automationPosition.asLeafTextPosition();

    if (!automationPosition.node ||
        automationPosition.node.role === RoleType.IMAGE) {
      // TODO(accessibility): Bugs in AXPosition cause this; for example, a
      // selection on a image has incorrect text offsets.
      return this.getDeepEquivalentForSelectionDeprecated(
          parent, offset, isStart);
    }

    return {
      node: automationPosition.node,
      offset: automationPosition.textOffset,
    };
  }

  /**
   * TODO(accessibility): remove once AXPosition bugs are fixed; see above.
   * @param parent The parent node of the selection,
   * similar to chrome.automation.selectionStartObject or selectionEndObject.
   * @param offset The integer offset of the selection. This is
   * similar to chrome.automation.selectionStartOffset or selectionEndOffset.
   * @param isStart whether this is the start or end of a selection.
   * @return The node matching the selected offset.
   */
  static getDeepEquivalentForSelectionDeprecated(
      parent: AutomationNode, offset: number,
      isStart: boolean): NodeUtils.Position {
    if (parent.children.length === 0) {
      return {node: parent, offset};
    }

    // Non-text nodes with children.
    if (parent.role !== RoleType.STATIC_TEXT &&
        parent.role !== RoleType.INLINE_TEXT_BOX &&
        parent.children.length > 0 && !NodeUtils.isTextField(parent)) {
      const index = isStart ? offset : offset - 1;
      if (parent.children.length > index && index >= 0) {
        let child = parent.children[index]!;
        if (child.children.length > 0) {
          child = isStart ? NodeUtils.getFirstLeafChild(child)! :
                            NodeUtils.getLastLeafChild(child)!;
        }
        return {
          node: child!,
          offset: isStart ? 0 : NodeUtils.nameLength(child!),
        };
      } else if (isStart && !NodeUtils.isTextField(parent)) {
        // We are off the edge of this parent. Go to the next leaf node that is
        // not an ancestor of the parent.
        const lastChild = NodeUtils.getLastLeafChild(parent);
        if (lastChild) {
          const nextNode = AutomationUtil.findNextNode(
              lastChild, constants.Dir.FORWARD, AutomationPredicate.leaf);
          if (nextNode) {
            return {node: nextNode, offset: 0};
          }
        }
      } else if (index < 0) {
        // Otherwise we are before the beginning of this parent. Find the
        // previous leaf node and use that.
        const previousNode = AutomationUtil.findNextNode(
            parent, constants.Dir.BACKWARD, AutomationPredicate.leaf);
        if (previousNode) {
          return {
            node: previousNode,
            offset: NodeUtils.nameLength(previousNode),
          };
        }
      }
    }

    // If this is a node without children or a text node, create a stack of
    // nodes to search through.
    // TODO(katie): Since we only have non-text nodes or nodes without children,
    // can this be simplified?
    let nodesToCheck: AutomationNode[];
    if (NodeUtils.isTextField(parent) && parent.firstChild &&
        parent.firstChild.firstChild) {
      // Skip ahead.
      nodesToCheck = parent.firstChild.children.slice().reverse();
    } else {
      nodesToCheck = parent.children.slice().reverse();
    }
    let index = 0;
    var node = parent;
    // Delve down into the children recursively to find the
    // one at this offset.
    while (nodesToCheck.length > 0) {
      node = nodesToCheck.pop()!;
      // TODO(b/314203187): Determine if not null assertion is appropriate here.
      if (node.state!['invisible']) {
        continue;
      }
      if (node.children.length > 0) {
        if (node.role !== RoleType.STATIC_TEXT) {
          index += 1;
        } else {
          nodesToCheck = nodesToCheck.concat(node.children.slice().reverse());
        }
      } else {
        if (node.parent!.role === RoleType.STATIC_TEXT ||
            node.parent!.role === RoleType.INLINE_TEXT_BOX) {
          // How many characters are in the name.
          index += NodeUtils.nameLength(node);
        } else {
          // Add one for itself only.
          index += 1;
        }
      }
      // Check if we've indexed far enough into the nodes of this parent to be
      // past the offset if |isStart|, or at the offset if !|isStart|.
      if (((isStart && index > offset) || (!isStart && index >= offset))) {
        // If the node is a text field type with children, return its first
        // (or last if !|isStart|) leaf child. Otherwise, just return the node
        // and its offset. textField nodes are indexed differently in selection
        // from others -- it seems as though the whole node counts only once in
        // the selection index if the textField is entirely selected, whereas a
        // normal staticText will count for itself plus one. This is probably
        // because textFields cannot be partially selected if other elements
        // outside of themselves are selected.
        if (NodeUtils.isTextField(node)) {
          const leafNode = isStart ? NodeUtils.getFirstLeafChild(node) :
                                     NodeUtils.getLastLeafChild(node);
          if (leafNode) {
            return {
              node: leafNode,
              offset: isStart ? 0 : NodeUtils.nameLength(leafNode),
            };
          }
        }
        const result = offset - index + NodeUtils.nameLength(node);
        return {node, offset: result > 0 ? result : 0};
      }
    }
    // We are at the end of the last node.
    // If it's a textField we skipped, go ahead and find the first (or last, if
    // !|isStart|) child, otherwise just return this node itself.
    if (NodeUtils.isTextField(node)) {
      const leafNode = isStart ? NodeUtils.getFirstLeafChild(node) :
                                 NodeUtils.getLastLeafChild(node);
      if (leafNode) {
        return {
          node: leafNode,
          offset: isStart ? 0 : NodeUtils.nameLength(leafNode),
        };
      }
    }
    return {node, offset: NodeUtils.nameLength(node)};
  }

  /**
   * Sorts given nodes by visual reading order. Expects nodes to be leaf nodes
   * with text.
   */
  static sortNodesByReadingOrder(nodes: AutomationNode[]): void {
    // Pre-compute ancestors for each node.
    const nodeAncestorMap = new Map();
    for (const node of nodes) {
      nodeAncestorMap.set(node, AutomationUtil.getAncestors(node));
    }

    // Sort nodes by bounds of their divergent ancestors. This will ensure all
    // nodes with the same parent are grouped together.
    nodes.sort((a, b) => {
      const ancestorsA = nodeAncestorMap.get(a);
      const ancestorsB = nodeAncestorMap.get(b);
      const divergence = AutomationUtil.getDivergence(ancestorsA, ancestorsB);
      if (divergence === -1 || divergence >= ancestorsA.length ||
          divergence >= ancestorsB.length) {
        // Nodes do not have any ancestors in common (different trees) or one
        // node is the ancestor of another.
        console.warn(
            'Nodes are directly related or have no common ancestors', a, b);
        return 0;
      }
      const divA = ancestorsA[divergence];
      const divB = ancestorsB[divergence];

      if (RectUtil.sameRow(divA.unclippedLocation, divB.unclippedLocation)) {
        // Nodes are on the same line, sort by LTR reading order.
        // TODO(joelriley@google.com): Handle RTL.
        if (divA.unclippedLocation.left < divB.unclippedLocation.left) {
          return -1;
        }
        if (divB.unclippedLocation.left < divA.unclippedLocation.left) {
          return 1;
        }
        return 0;
      }
      // Nodes are on different lines, sort top-to-bottom.
      if (divA.unclippedLocation.top < divB.unclippedLocation.top) {
        return -1;
      }
      if (divB.unclippedLocation.top < divA.unclippedLocation.top) {
        return 1;
      }
      return 0;
    });
  }

  /**
   * Sorts a specific range of a given array of nodes by visual reading order.
   * Expects nodes to be leaf nodes with text.
   * @param startIndex Index specifying start of range.
   * @param endIndex  Index specifying end of range, non-inclusive.
   */
  static sortNodeRangeByReadingOrder(
      nodes: AutomationNode[], startIndex: number, endIndex: number): void {
    const nodesToSort = nodes.slice(startIndex, endIndex);
    NodeUtils.sortNodesByReadingOrder(nodesToSort);
    nodes.splice(startIndex, endIndex - startIndex, ...nodesToSort);
  }

  /**
   * Sorts SVG nodes with the same SVG root parent by visual reading order.
   */
  static sortSvgNodesByReadingOrder(nodes: AutomationNode[]): void {
    let lastSvgRoot = null;
    let startIndex = 0;
    for (let i = 0; i < nodes.length; i++) {
      const node = nodes[i];
      const svgRoot =
          AutomationUtil.getFirstAncestorWithRole(node, RoleType.SVG_ROOT);
      if (svgRoot !== lastSvgRoot) {
        if (lastSvgRoot !== null) {
          NodeUtils.sortNodeRangeByReadingOrder(nodes, startIndex, i);
        } else if (svgRoot !== null) {
          startIndex = i;
        }
        lastSvgRoot = svgRoot;
      }
    }
    if (lastSvgRoot !== null) {
      NodeUtils.sortNodeRangeByReadingOrder(nodes, startIndex, nodes.length);
    }
  }

  /**
   * @param node Leaf node.
   * @return All selectable leaf nodes in the paragraph that the given
   *     leaf node belongs to. If the node does not belong to a paragraph,
   *     then just the node itself is returned.
   */
  static getAllNodesInParagraph(node: AutomationNode): AutomationNode[] {
    const blockParent = ParagraphUtils.getFirstBlockAncestor(node);
    if (blockParent === null || blockParent === node.root) {
      return [node];
    }
    return AutomationUtil.findAllNodes(
        blockParent, constants.Dir.FORWARD,
        /* pred= */ NodeUtils.isValidLeafNode, /* opt_restrictions= */ {
          root: node => node === blockParent,  // Only traverse within the block
        });
  }

  /**
   * Gets the |NodeUtils.Position| identified by the |charIndex| to the text of
   * |nodeGroup|. If |fallbackToEnd| is true, when the |charIndex| is undefined
   * or out of the text of |nodeGroup|, we will return the end of the
   * |nodeGroup|. Otherwise, we fallback to the start of the |nodeGroup|.
   */
  static getPositionFromNodeGroup(
      nodeGroup: ParagraphUtils.NodeGroup, charIndex: number|undefined,
      fallbackToEnd: boolean): NodeUtils.Position {
    let node;
    let offset;
    if (charIndex !== undefined) {
      ({node, offset} = ParagraphUtils.findNodeFromNodeGroupByCharIndex(
           nodeGroup, charIndex));
    }
    if (node && offset !== undefined) {
      return {node, offset};
    }

    // |charIndex| is undefined or out of the text of |nodeGroup|, fallback to
    // the end or the start of the |nodeGroup|.
    if (fallbackToEnd) {
      const lastNode = nodeGroup.nodes[nodeGroup.nodes.length - 1].node;
      const lastChildOfLastNode = NodeUtils.getLastLeafChild(lastNode);
      node = lastChildOfLastNode || lastNode;
      offset = ParagraphUtils.getNodeName(node).length;
    } else {
      const firstNode = nodeGroup.nodes[0].node;
      const firstChildOfFirstNode = NodeUtils.getFirstLeafChild(firstNode);
      node = firstChildOfFirstNode || firstNode;
      offset = 0;
    }
    return {node, offset};
  }

  /**
   * @param node
   * @return Whether the given node is a valid leaf node that is can
   *     be ingested by Select-to-speak.
   */
  static isValidLeafNode(node: AutomationNode): boolean {
    return AutomationPredicate.leafWithText(node) &&
        !NodeUtils.shouldIgnoreNode(node, /* includeOffscreen= */ true) &&
        !NodeUtils.isNotSelectable(node);
  }

  /**
   * @return The direction from the |startPosition| to the
   *     |endPosition|. If the input positions are equal, we view the
   *     |endPosition| is to the |constants.Dir.BACKWARD| of the
   *     |startPosition|.
   */
  static getDirectionBetweenPositions(
      startPosition: NodeUtils.Position,
      endPosition: NodeUtils.Position): constants.Dir {
    const startNode = startPosition.node;
    const startOffset = startPosition.offset;
    const endNode = endPosition.node;
    const endOffset = endPosition.offset;
    if (startNode !== endNode) {
      return AutomationUtil.getDirection(startNode, endNode);
    }
    if (startOffset < endOffset) {
      return constants.Dir.FORWARD;
    }
    return constants.Dir.BACKWARD;
  }
}

export namespace NodeUtils {
  /**
   * Node state. Nodes can be on-screen like normal, or they may
   * be invisible if they are in a tab that is not in the foreground
   * or similar, or they may be invalid if they were removed from their
   * root, i.e. if they were in a window that was closed.
   */
  export enum NodeState {
    NODE_STATE_INVALID = 0,
    NODE_STATE_INVISIBLE = 1,
    NODE_STATE_NORMAL = 2,
  }


  /**
   * Class representing a position on the accessibility, made of a
   * selected node and the offset of that selection.
   */
  export interface Position {
    node: AutomationNode;
    offset: number;
  }
}
