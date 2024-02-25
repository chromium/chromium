// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutomationUtil} from './automation_util.js';
import {constants} from './constants.js';
import {NodeUtils} from './node_utils.js';
import {ParagraphUtils} from './paragraph_utils.js';
import {SentenceUtils} from './sentence_utils.js';

import AutomationNode = chrome.automation.AutomationNode;
const RoleType = chrome.automation.RoleType;

/**
 * A predicate for paragraph selection. The predicate examines an array of nodes
 * and returns false if the paragraph should not be used.
 */
interface ParagraphPred {
  (nodes: AutomationNode[]): boolean;
}

/**
 * Utility functions to handle sentence navigation and paragraph navigation.
 * These functions are based on NodeUtils, ParagraphUtils, and SentenceUtils,
 * which handle lower-level calculation.
 */
export class NodeNavigationUtils {
  /**
   * Finds the nodes for the next text block in the given direction. This
   * function is based on |NodeUtils.getNextParagraph| but provides additional
   * checks on the anchor node used for searchiong. This function also applies a
   * |pred| to filter out unqualified paragraph.
   * @param pred A predicate to apply when selecting the
   *     paragraph. After querying nodes, the function uses |pred| to determine
   *     if the queried nodes are a part of a valid paragraph. If |pred| returns
   *     false, the function returns an empty array. For example, we may want to
   *     discard nodes from UI components.
   * @return A list of nodes for the next block in the given direction.
   */
  static getNodesForNextParagraph(
      currentNodeGroup: ParagraphUtils.NodeGroup|undefined,
      direction: constants.Dir, pred: ParagraphPred): AutomationNode[] {
    if (!currentNodeGroup) {
      return [];
    }
    // Use current block parent as starting point to navigate from. If it is not
    // a valid block, then use one of the nodes that are currently activated.
    let node = currentNodeGroup.blockParent;
    if ((!node || node.isRootNode || node.role === undefined) &&
        currentNodeGroup.nodes.length > 0) {
      node = currentNodeGroup.nodes[0].node;
    }
    if (!node || node.role === undefined) {
      // Could not find any nodes to navigate from.
      return [];
    }

    // Retrieve the nodes that make up the next/prev paragraph.
    const nextParagraphNodes =
        NodeNavigationUtils.getNextParagraphWithNode_(node, direction);
    if (nextParagraphNodes.length === 0 || !pred(nextParagraphNodes)) {
      // Cannot find any valid nodes in given direction.
      return [];
    }

    return nextParagraphNodes;
  }

  /**
   * @param node Node to traverse from.
   * @param direction Direction to traverse.
   * @return Returns all selectable leaf text nodes within the paragraph
   *     adjacent to the given node. If there is an adjacent valid leaf
   *     node not contained within a paragraph, it will return that node.
   *     Only traverses within containing root.
   */
  private static getNextParagraphWithNode_(
      node: AutomationNode, direction: constants.Dir): AutomationNode[] {
    const blockParent = ParagraphUtils.isBlock(node) ?
        node :
        ParagraphUtils.getFirstBlockAncestor(node);
    // TODO(b/314203187): Determine if not null assertion is appropriate here.
    let containingRoot = node.root!;
    // If role is a rootWebArea, search for the first ancestor with that role,
    // to enable users to traverse across iframes within the same webpage.
    if (containingRoot.role === RoleType.ROOT_WEB_AREA) {
      const ancestors = AutomationUtil.getAncestors(containingRoot);
      const topRootWebArea =
          ancestors.find(a => a.role === RoleType.ROOT_WEB_AREA);
      if (topRootWebArea) {
        containingRoot = topRootWebArea;
      }
    }
    let startNode = blockParent;
    if (startNode === null || startNode === node.root) {
      startNode = node;
    }
    const nextNode = AutomationUtil.findNextNode(
        startNode, direction, NodeUtils.isValidLeafNode, {
          root: n => containingRoot === n,
          skipInitialSubtree: true,
        });
    if (nextNode === null) {
      return [];
    }

    const nextNodes =
        NodeNavigationUtils.getNextNodesInParagraph_(nextNode, direction);
    if (direction === constants.Dir.FORWARD) {
      nextNodes.unshift(nextNode);
    } else {
      nextNodes.push(nextNode);
    }
    return nextNodes;
  }

  /**
   * Gets the remaining content of a paragraph with an assigned position. The
   * position is defined by the |charIndex| to the text of |nodeGroup|. If
   * |direction| is set to forward, we will look for trailing content after the
   * position. Otherwise, we will get the leading content before the position.
   * The remaining content is returned as a list of nodes with offset.
   * @param nodeGroup The nodeGroup of the assigned position. The nodeGroup may
   *     contain the content of the entire paragraph or only a part of the
   *     paragraph.
   * @param charIndex The char index of the position. The index is
   *     relative to the text content of the |nodeGroup|. This index is
   *     inclusive for forward searching: if we set |direction| to forward with
   *     a 0 |charIndex|, we will get the remaining content of the paragraph
   *     including all the content in the input |nodeGroup|. However, it is
   *     exclusive for backward searching: when searching backward with a 0
   *     |charIndex|, we will exclude all the content in the input |nodeGroup|.
   * @return
   *    nodes: the nodes that have the remaining content.
   *    offset: the offset for the nodes. See more details in
   *    |NodeNavigationUtils.getNextNodesInParagraphFromPosition|.
   */
  static getNextNodesInParagraphFromNodeGroup(
      nodeGroup: ParagraphUtils.NodeGroup, charIndex: number,
      direction: constants.Dir): {nodes: AutomationNode[], offset: number} {
    if (nodeGroup.nodes.length === 0) {
      return {nodes: [], offset: -1};
    }
    // Get the current position. When searching forward, if we did not find a
    // node for the charIndex, we fallback to the end of the current node group.
    // This enables us to get all the content within this paragraph but after
    // the current node group. When searching backward, if we did not find a
    // node for the charIndex, we fallback to the start of the current node
    // group. This enables us to get all the content before the current node
    // group in this paragraph.
    const fallbackToEnd = direction === constants.Dir.FORWARD;
    const currentPosition =
        NodeUtils.getPositionFromNodeGroup(nodeGroup, charIndex, fallbackToEnd);
    return NodeNavigationUtils.getNextNodesInParagraphFromPosition(
        currentPosition, direction);
  }

  /**
   * Gets the remaining content of a paragraph with an assigned |position|. If
   * |direction| is set to forward, we will look for trailing content after the
   * position. Otherwise, we will get the leading content before the position.
   * The remaining content is returned as a list of nodes with offset.
   * @return
   *    nodes: the nodes that have the remaining content.
   *    offset: the offset for the nodes. When searching forward, the offset is
   * into the name of the first node, and marks the start index of the remaining
   * content in the first node, inclusively. For example, "Hello" with an offset
   * 3 indicates that the remaining content is "lo". Otherwise, the offset is
   * into the name of the last node, and marks the end index of the remaining
   * content in the last node, exclusively. For example, "Hello" with an offset
   * 3 indicates that the remaining content is "Hel".
   */
  static getNextNodesInParagraphFromPosition(
      position: NodeUtils.Position,
      direction: constants.Dir): {nodes: AutomationNode[], offset: number} {
    const startNode = position.node;
    const offset = position.offset;
    if (direction === constants.Dir.BACKWARD) {
      // Gets all the nodes before the startNode. We include the start node
      // if it is not empty. Note that this is based on the assumption that
      // this function will still include nodes with overflow text.
      const leadingNodes = NodeNavigationUtils.getNextNodesInParagraph_(
          startNode, constants.Dir.BACKWARD);
      const textInStartNode =
          ParagraphUtils.getNodeName(startNode).substr(0, offset);
      if (!ParagraphUtils.isWhitespace(textInStartNode)) {
        leadingNodes.push(startNode);
        return {nodes: leadingNodes, offset};
      }
      if (leadingNodes.length === 0) {
        return {nodes: [], offset: -1};
      }
      // Returns all the nodes once we find a valid one among them.
      for (const node of leadingNodes) {
        if (NodeUtils.isValidLeafNode(node)) {
          const lastLeadingNodeName =
              ParagraphUtils.getNodeName(leadingNodes[leadingNodes.length - 1]);
          return {nodes: leadingNodes, offset: lastLeadingNodeName.length};
        }
      }
      return {nodes: [], offset: -1};
    }

    // Gets all the trailing nodes after the startNode. We include the start
    // node if it is not empty. Note that this is based on the assumption that
    // this function will still include nodes with overflow text.
    const trailingNodes = NodeNavigationUtils.getNextNodesInParagraph_(
        startNode, constants.Dir.FORWARD);
    const textInStartNode =
        ParagraphUtils.getNodeName(startNode).substr(offset);
    if (!ParagraphUtils.isWhitespace(textInStartNode)) {
      const nodes = [startNode, ...trailingNodes];
      return {nodes, offset};
    }
    if (trailingNodes.length === 0) {
      return {nodes: [], offset: -1};
    }
    // Returns all the nodes once we find a valid one among them.
    for (const node of trailingNodes) {
      if (NodeUtils.isValidLeafNode(node)) {
        return {nodes: trailingNodes, offset: 0};
      }
    }
    return {nodes: [], offset: -1};
  }

  /**
   * @param node Leaf node.
   * @return The selectable leaf nodes in the given
   *     direction from the given node, until a paragraph break is reached.
   */
  private static getNextNodesInParagraph_(
      node: AutomationNode, direction: constants.Dir): AutomationNode[] {
    const blockParent = ParagraphUtils.getFirstBlockAncestor(node);
    if (blockParent === null || blockParent === node.root) {
      return [];
    }
    const nodes = AutomationUtil.findAllNodes(
        node, direction,
        /* pred= */ NodeUtils.isValidLeafNode, /* opt_restrictions= */ {
          root: node => node === blockParent,  // Only traverse within the block
        });

    // Reverse the nodes if we were traversing backward, so the returned result
    // is in natural DOM order.
    return direction === constants.Dir.BACKWARD ? nodes.reverse() : nodes;
  }

  /**
   * Gets the nodes for the next sentence. First, we search the next sentence in
   * the current node group. If we do not find one, we will search within the
   * remaining content in the current paragraph (i.e., text block). If this
   * still fails, we will search the next paragraph. The current position is
   * defined by the |currentCharIndex| in the |currentNodeGroup|. When
   * navigating backwards, we skip the sentence start in the current sentence.
   * For example, when navigating backward from the middle of the current
   * sentence, the function returns content from the start of the previous
   * sentence.
   * TODO(leileilei@google.com): Handle the edge case where the user navigates
   * to next sentence from the end of a document, see http://crbug.com/1160962.
   * @param direction Direction to search for the next sentence.
   *     If set to forward, we look for the sentence start after the current
   *     position. Otherwise, we look for the sentence start before the current
   *     position.
   * @param pred A predicate to apply when selecting the
   *     paragraph. See |NodeNavigationUtils.getNodesForNextParagraph| for
   * details.
   * @return
   *     nodes: A list of nodes for the next block in the given direction.
   *     offset: The start offset for the found sentence.
   */
  static getNodesForNextSentence(
      currentNodeGroup: ParagraphUtils.NodeGroup|undefined,
      currentCharIndex: number, direction: constants.Dir, pred: ParagraphPred):
      {nodes: AutomationNode[], offset: (number|undefined)} {
    let nodes: AutomationNode[] = [];
    let offset;
    if (!currentNodeGroup) {
      return {nodes, offset};
    }

    // Sets |skipCurrentSentence| to true for initial backward navigation.
    let skipCurrentSentence = direction === constants.Dir.BACKWARD;

    // Checks the next sentence within this node group. If we can find the
    // next sentence that fulfilled the requirements, return that.
    ({nodes, offset} =
         NodeNavigationUtils.getNodesForNextSentenceWithinNodeGroup_(
             currentNodeGroup, currentCharIndex, direction, pred,
             skipCurrentSentence));
    if (nodes.length > 0) {
      return {nodes, offset};
    }

    // If there is no next sentence at the current node group, look for the
    // content within this paragraph. First, we get the remaining content in
    // the paragraph. The returned offset marks the char index of the current
    // position in the paragraph. When searching forward, the offset is the
    // char index pointing to the beginning of the remaining content. When
    // searching backward, the offset is the char index pointing to the char
    // after the remaining content.
    ({nodes, offset} = NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
         currentNodeGroup, currentCharIndex, direction));
    // If we have reached to the end of the current paragraph, return the
    // sentence from the next paragraph.
    if (nodes.length === 0) {
      return NodeNavigationUtils.getNodesForNextSentenceInNextParagraph_(
          currentNodeGroup, direction, pred);
    }
    // Get the node group for the remaining content in the paragraph. If we are
    // looking for the content after the current position, set startIndex as
    // offset. Otherwise, set endIndex as offset.
    const startIndex = direction === constants.Dir.FORWARD ? offset : undefined;
    const endIndex = direction === constants.Dir.FORWARD ? undefined : offset;
    const {nodeGroup, startIndexInGroup, endIndexInGroup} =
        ParagraphUtils.buildSingleNodeGroupWithOffset(
            nodes, startIndex, endIndex);
    // Search in the remaining content.
    const charIndex = direction === constants.Dir.FORWARD ? startIndexInGroup :
                                                            endIndexInGroup;
    // The charIndex is guaranteed to be valid at this point, although the
    // closure compiler is not able to detect it as a valid number.
    if (charIndex === undefined) {
      console.warn('Navigate sentence with an invalid char index', charIndex);
      return {nodes: [], offset: undefined};
    }
    // When searching backward, we need to adjust |skipCurrentSentence|. The
    // remaining content we get excludes the char at |currentCharIndex|. If this
    // char is a sentence start, we have already skipped the current sentence so
    // we need to change |skipCurrentSentence| to false for the next search.
    if (direction === constants.Dir.BACKWARD && skipCurrentSentence) {
      const currentPositionIsSentenceStart =
          SentenceUtils.isSentenceStart(currentNodeGroup, currentCharIndex);
      if (currentPositionIsSentenceStart) {
        skipCurrentSentence = false;
      }
    }
    ({nodes, offset} =
         NodeNavigationUtils.getNodesForNextSentenceWithinNodeGroup_(
             nodeGroup, charIndex, direction, pred, skipCurrentSentence));
    if (nodes.length > 0) {
      return {nodes, offset};
    }

    // If there is no next sentence within this paragraph, enqueue the sentence
    // from the next paragraph.
    return NodeNavigationUtils.getNodesForNextSentenceInNextParagraph_(
        currentNodeGroup, direction, pred);
  }

  /**
   * Gets the nodes for the next sentence within the |nodeGroup|. If the
   * |direction| is set to forward, it will navigate to the sentence start after
   * the |startCharIndex|. Otherwise, it will look for the sentence start before
   * the |startCharIndex|.
   * @param startCharIndex The char index that we start from. This
   *     index is relative to the text content of this node group and is
   *     exclusive: if a sentence start at 0 and we search with a 0
   *     |startCharIndex|, this function will return the next sentence start
   *     after 0 if we search forward.
   * @param pred A predicate to apply when selecting the
   *     paragraph. See |NodeNavigationUtils.getNodesForNextParagraph| for
   * details.
   * @param skipCurrentSentence Whether to skip the current sentence
   *     when navigating backward. Please refer to more details in the
   *     |NodeNavigationUtils.getNodesForNextSentence|.
   * @return
   *     nodes: A list of nodes for the next block in the given direction.
   *     offset: The start offset for the found sentence.
   */
  private static getNodesForNextSentenceWithinNodeGroup_(
      nodeGroup: ParagraphUtils.NodeGroup, startCharIndex: number,
      direction: constants.Dir, pred: ParagraphPred,
      skipCurrentSentence: boolean):
      {nodes: AutomationNode[], offset: (number|undefined)} {
    if (!nodeGroup) {
      return {nodes: [], offset: undefined};
    }
    let nextSentenceStart =
        SentenceUtils.getSentenceStart(nodeGroup, startCharIndex, direction);
    if (nextSentenceStart === null) {
      return {nodes: [], offset: undefined};
    }
    // When we search backward, if we want to skip the current sentence, we
    // need to search the sentence start in the previous sentence. If the
    // position of |startCharIndex| is a sentence start, the current
    // |nextSentenceStart| is already in the previous sentence because
    // getSentenceStart excludes the search index. Otherwise, the
    // |nextSentenceStart| we found is the start of current sentence, and we
    // need to search backward again.
    if (direction === constants.Dir.BACKWARD && skipCurrentSentence &&
        !SentenceUtils.isSentenceStart(nodeGroup, startCharIndex)) {
      nextSentenceStart = SentenceUtils.getSentenceStart(
          nodeGroup, nextSentenceStart, direction);
    }
    // If the second sentence start is not valid, returns empty nodes.
    if (nextSentenceStart === null) {
      return {nodes: [], offset: undefined};
    }

    // Get the content between the sentence start and the end of the paragraph.
    const {nodes, offset} =
        NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
            nodeGroup, nextSentenceStart, constants.Dir.FORWARD);
    if (nodes.length === 0) {
      // There is no remaining content. Move to the next paragraph. This is
      // unexpected since we already found a sentence start, which indicates
      // there should be some content to read.
      return NodeNavigationUtils.getNodesForNextSentenceInNextParagraph_(
          nodeGroup, direction, pred);
    }

    return {nodes, offset};
  }

  /**
   * Gets the nodes for the next sentence in the next text block in the given
   * direction. If the |direction| is set to forward, it will navigate to the
   * start of the following text block. Otherwise, it will look for the last
   * sentence in the previous text block.
   * @param pred A predicate to apply when selecting the paragraph. See
   *   |NodeNavigationUtils.getNodesForNextParagraph| for details.
   * @return
   *     nodes: A list of nodes for the next block in the given direction.
   *     offset: The start offset for the found sentence.
   */
  private static getNodesForNextSentenceInNextParagraph_(
      currentNodeGroup: ParagraphUtils.NodeGroup|undefined,
      direction: constants.Dir, pred: ParagraphPred):
      {nodes: AutomationNode[], offset: (number|undefined)} {
    const paragraphNodes = NodeNavigationUtils.getNodesForNextParagraph(
        currentNodeGroup, direction, pred);
    // Return early if the nodes are empty.
    if (paragraphNodes.length === 0) {
      return {nodes: paragraphNodes, offset: undefined};
    }

    if (direction === constants.Dir.FORWARD) {
      // If we are looking for the sentence start in the following text block,
      // return nodes.
      return {nodes: paragraphNodes, offset: undefined};
    }

    // If we are looking for the previous sentence start, search the last
    // sentence in the previous text block. Get the node group for the previous
    // text block. The returned startIndexInGroup and endIndexInGroup are
    // unused.
    const {nodeGroup} =
        ParagraphUtils.buildSingleNodeGroupWithOffset(paragraphNodes);
    // We search backward for the sentence start before the end of the text
    // block.
    const searchOffset = nodeGroup.text.length;
    const sentenceStartIndex = SentenceUtils.getSentenceStart(
        nodeGroup, searchOffset, constants.Dir.BACKWARD);
    // If there is no sentence start in the previous text block, return the
    // nodes of the block.
    if (sentenceStartIndex === null) {
      return {nodes: paragraphNodes, offset: undefined};
    }
    // Gets the remaining content between the sentence start until the end of
    // the text block. The offset is the start char index for the first node in
    // the remaining content.
    const {nodes, offset} =
        NodeNavigationUtils.getNextNodesInParagraphFromNodeGroup(
            nodeGroup, sentenceStartIndex, constants.Dir.FORWARD);
    if (nodes.length === 0) {
      // If there is no remaining content, return the nodes of the block. This
      // is unexpected since we already found a sentence start, which indicates
      // there should be some content to read.
      return {nodes: paragraphNodes, offset: undefined};
    }
    // Returns the remaining content from the sentence start until the end of
    // the block.
    return {nodes, offset};
  }
}
