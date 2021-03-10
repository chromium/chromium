// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NodeUtils} from './node_utils.js';
import {ParagraphUtils} from './paragraph_utils.js';
import {SentenceUtils} from './sentence_utils.js';

const AutomationNode = chrome.automation.AutomationNode;

/**
 * A predicate for paragraph selection. The predicate examines an array of nodes
 * and returns false if the paragraph should not be used.
 * @typedef {function(Array<!AutomationNode>): boolean}
 */
let ParagraphPred;

/**
 * Utility functions to handle sentence navigation and paragraph navigation.
 * TODO(crbug.com/1168644): move other navigation-only utility functions to this
 * file.
 */
export class NodeNavigationUtils {
  constructor() {}

  /**
   * Finds the nodes for the next text block in the given direction. This
   * function is based on |NodeUtils.getNextParagraph| but provides additional
   * checks on the anchor node used for searchiong. This function also applies a
   * |pred| to filter out unqualified paragraph.
   * @param {!ParagraphUtils.NodeGroup|undefined} currentNodeGroup
   * @param {constants.Dir} direction
   * @param {ParagraphPred} pred A predicate to apply when selecting the
   *     paragraph. After querying nodes, the function uses |pred| to determine
   *     if the queried nodes are a part of a valid paragraph. If |pred| returns
   *     false, the function returns an empty array. For example, we may want to
   *     discard nodes from UI components.
   * @return {Array<!AutomationNode>} A list of nodes for the next block in the
   *     given direction.
   */
  static getNodesForNextParagraph(currentNodeGroup, direction, pred) {
    if (!currentNodeGroup) {
      return [];
    }
    // Use current block parent as starting point to navigate from. If it is not
    // a valid block, then use one of the nodes that are currently activated.
    let node = currentNodeGroup.blockParent;
    if ((node === null || node.isRootNode || node.role === undefined) &&
        currentNodeGroup.nodes.length > 0) {
      node = currentNodeGroup.nodes[0].node;
    }
    if (node === null || node.role === undefined) {
      // Could not find any nodes to navigate from.
      return [];
    }

    // Retrieve the nodes that make up the next/prev paragraph.
    const nextParagraphNodes = NodeUtils.getNextParagraph(node, direction);
    if (nextParagraphNodes.length === 0 || !pred(nextParagraphNodes)) {
      // Cannot find any valid nodes in given direction.
      return [];
    }

    return nextParagraphNodes;
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
   * @param {!ParagraphUtils.NodeGroup|undefined} currentNodeGroup
   * @param {number} currentCharIndex
   * @param {constants.Dir} direction Direction to search for the next sentence.
   *     If set to forward, we look for the sentence start after the current
   *     position. Otherwise, we look for the sentence start before the current
   *     position.
   * @param {ParagraphPred} pred A predicate to apply when selecting the
   *     paragraph. See |NodeNavigationUtils.getNodesForNextParagraph| for
   * details.
   * @return {!{nodes: Array<!AutomationNode>,
   *          offset: (number|undefined)}}
   *     nodes: A list of nodes for the next block in the given direction.
   *     offset: The start offset for the found sentence.
   */
  static getNodesForNextSentence(
      currentNodeGroup, currentCharIndex, direction, pred) {
    let nodes = [], offset;
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
    ({nodes, offset} = NodeUtils.getNextNodesInParagraphFromNodeGroup(
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
   * @param {ParagraphUtils.NodeGroup} nodeGroup
   * @param {number} startCharIndex The char index that we start from. This
   *     index is relative to the text content of this node group and is
   *     exclusive: if a sentence start at 0 and we search with a 0
   *     |startCharIndex|, this function will return the next sentence start
   *     after 0 if we search forward.
   * @param {constants.Dir} direction
   * @param {ParagraphPred} pred A predicate to apply when selecting the
   *     paragraph. See |NodeNavigationUtils.getNodesForNextParagraph| for
   * details.
   * @param {boolean} skipCurrentSentence Whether to skip the current sentence
   *     when navigating backward. Please refer to more details in the
   *     |NodeNavigationUtils.getNodesForNextSentence|.
   * @return {!{nodes: Array<!AutomationNode>,
   *          offset: (number|undefined)}}
   *     nodes: A list of nodes for the next block in the given direction.
   *     offset: The start offset for the found sentence.
   * @private
   */
  static getNodesForNextSentenceWithinNodeGroup_(
      nodeGroup, startCharIndex, direction, pred, skipCurrentSentence) {
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
    const {nodes, offset} = NodeUtils.getNextNodesInParagraphFromNodeGroup(
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
   * @param {!ParagraphUtils.NodeGroup|undefined} currentNodeGroup
   * @param {constants.Dir} direction
   * @param {ParagraphPred} pred A predicate to apply when selecting the
   *     paragraph. See |NodeNavigationUtils.getNodesForNextParagraph| for
   * details.
   * @return {!{nodes: Array<!AutomationNode>,
   *          offset: (number|undefined)}}
   *     nodes: A list of nodes for the next block in the given direction.
   *     offset: The start offset for the found sentence.
   * @private
   */
  static getNodesForNextSentenceInNextParagraph_(
      currentNodeGroup, direction, pred) {
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
    const {nodeGroup, startIndexInGroup, endIndexInGroup} =
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
    const {nodes, offset} = NodeUtils.getNextNodesInParagraphFromNodeGroup(
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