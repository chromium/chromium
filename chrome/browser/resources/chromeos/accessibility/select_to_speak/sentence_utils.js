// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Utilities for processing sentences within strings and node groups.
 */
class SentenceUtils {
  constructor() {}

  /**
   * Get the next sentence start. When |direction| is set to forward, this
   * function will incrementally go over each nodeGroupItem in |nodeGroup|,
   * and try to find a sentence start index after the |startCharIndex|. When
   * |direction| is set to backward, This function will search for the preivous
   * sentence. The |startCharIndex| and the found sentence start index are
   * relative to the start of this node group.
   * @param {ParagraphUtils.NodeGroup} nodeGroup The node group this function
   *     will search.
   * @param {number} startCharIndex The char index that we start from. This
   *     index is relative to the start of this node group and is exclusive: if
   *     a sentence start at 0 and we search with a 0 |startCharIndex|, this
   *     function will return the next sentence start after 0.
   * @param {constants.Dir} direction Direction for the next sentence.
   *     |constants.Dir.BACKWARD| will search for the previous sentence while
   *     |constants.Dir.FORWARD| will search for the next sentence.
   * @return {?number} the next sentence start after |startCharIndex|, returns
   *     null if nothing found.
   */
  static getSentenceStart(nodeGroup, startCharIndex, direction) {
    if (nodeGroup.nodes.length === 0) {
      return null;
    }
    if (direction === constants.Dir.FORWARD) {
      for (let i = 0; i < nodeGroup.nodes.length; i++) {
        const nodeGroupItem = nodeGroup.nodes[i];
        const result = SentenceUtils.getNextSentenceStartInNodeGroupItem(
            nodeGroupItem, startCharIndex);
        if (result !== null) {
          return result;
        }
      }
    } else if (direction === constants.Dir.BACKWARD) {
      for (let i = nodeGroup.nodes.length - 1; i >= 0; i--) {
        const nodeGroupItem = nodeGroup.nodes[i];
        const result = SentenceUtils.getPrevSentenceStartInNodeGroupItem(
            nodeGroupItem, startCharIndex);
        if (result !== null) {
          return result;
        }
      }
    }
    return null;
  }

  /**
   * Get the next sentence start within a node group item.
   * @param {ParagraphUtils.NodeGroupItem} nodeGroupItem The node group item
   *     this function will search.
   * @param {number} startCharIndex The char index that we start from.
   * @return {?number} the next sentence start after |startCharIndex|, returns
   *     null if nothing found.
   */
  static getNextSentenceStartInNodeGroupItem(nodeGroupItem, startCharIndex) {
    const nodeGroupItemHasInlineText =
        nodeGroupItem && nodeGroupItem.hasInlineText;
    if (!nodeGroupItemHasInlineText) {
      return null;
    }
    const nodeGroupItemHasContent = nodeGroupItem.node.children.length > 0 &&
        nodeGroupItem.node.name.length > 0;
    if (!nodeGroupItemHasContent) {
      return null;
    }
    const staticTextStartChar = nodeGroupItem.startChar;
    const staticTextNode = nodeGroupItem.node;

    // If the corresponding char of |startCharIndex| is after this static text
    // node, skip this text node.
    if (startCharIndex > staticTextStartChar + staticTextNode.name.length - 1) {
      return null;
    }
    // If the corresponding char of |startCharIndex| is within this static text
    // node, we get its relative index in this static text. Otherwise, we search
    // from the beginning of the static text.
    const searchIndexInStaticText =
        Math.max(startCharIndex - staticTextStartChar, 0);

    // Find the index of the inline text node corresponding to the
    // |searchIndexInStaticText|.
    const inlineTextNodeIndex =
        ParagraphUtils.findInlineTextNodeIndexByCharacterIndex(
            staticTextNode, searchIndexInStaticText);
    // findInlineTextNodeIndexByCharacterIndex may return a negative number,
    // indicating no children in the staticTextNode.
    if (inlineTextNodeIndex < 0) {
      return null;
    }

    // Incrementally iterate over each inline text nodes within this static text
    // node from |inlineTextNodeIndex|.
    for (let inlineTextNode, startCharInStaticText, i = inlineTextNodeIndex;
         i < staticTextNode.children.length; i++) {
      // If this is the first node in the for loop, use ParagraphUtils to get
      // the start char. Otherwise, update the start char by adding the name
      // length of previous node.
      if (i === inlineTextNodeIndex) {
        inlineTextNode = staticTextNode.children[i];
        startCharInStaticText =
            ParagraphUtils.getStartCharIndexInParent(inlineTextNode);
      } else {
        startCharInStaticText += inlineTextNode.name.length;
        inlineTextNode = staticTextNode.children[i];
      }

      // Continue to the next one if the end of this inlineTextNode is still
      // smaller than startCharIndex.
      if (inlineTextNode.name.length - 1 + startCharInStaticText +
              staticTextStartChar <=
          startCharIndex) {
        continue;
      }

      // Iterate over all sentenceStarts to find the one that is bigger than
      // startCharIndex.
      for (let j = 0; j < inlineTextNode.sentenceStarts.length; j++) {
        const potentialStart = inlineTextNode.sentenceStarts[j] +
            startCharInStaticText + staticTextStartChar;
        if (potentialStart <= startCharIndex) {
          continue;
        }
        return potentialStart;
      }
    }
    // We are off the edge of this static text node, return null.
    return null;
  }

  /**
   * Get the previous sentence start within a node group item.
   * @param {ParagraphUtils.NodeGroupItem} nodeGroupItem The node group item
   *     this function will search.
   * @param {number} startCharIndex The char index that we start from.
   * @return {?number} the previous sentence start before |startCharIndex|,
   *     returns null if nothing found.
   */
  static getPrevSentenceStartInNodeGroupItem(nodeGroupItem, startCharIndex) {
    const nodeGroupItemHasInlineText =
        nodeGroupItem && nodeGroupItem.hasInlineText;
    if (!nodeGroupItemHasInlineText) {
      return null;
    }
    const nodeGroupItemHasContent = nodeGroupItem.node.children.length > 0 &&
        nodeGroupItem.node.name.length > 0;
    if (!nodeGroupItemHasContent) {
      return null;
    }
    const staticTextStartChar = nodeGroupItem.startChar;
    const staticTextNode = nodeGroupItem.node;

    // If the corresponding char of |startCharIndex| is before this static text
    // node, skip this text node.
    if (startCharIndex < staticTextStartChar) {
      return null;
    }
    // If the corresponding char of |startCharIndex| is within this static text
    // node, we get its relative index in this static text. Otherwise, we search
    // from the end of the static text.
    const searchIndexInStaticText = Math.min(
        startCharIndex - staticTextStartChar, staticTextNode.name.length - 1);

    // Find the index of the inline text node corresponding to the
    // |searchIndexInStaticText|.
    const inlineTextNodeIndex =
        ParagraphUtils.findInlineTextNodeIndexByCharacterIndex(
            staticTextNode, searchIndexInStaticText);
    // findInlineTextNodeIndexByCharacterIndex may return a negative number,
    // indicating no children in the staticTextNode.
    if (inlineTextNodeIndex < 0) {
      return null;
    }

    // Iterate backwards over each inline text nodes within this static text
    // node from |inlineTextNodeIndex|.
    for (let inlineTextNode, startCharInStaticText, i = inlineTextNodeIndex;
         i >= 0; i--) {
      inlineTextNode = staticTextNode.children[i];
      // If this is the first node in the for loop, use ParagraphUtils to get
      // the start char. Otherwise, update the start char by subtracting the
      // name length of the current node.
      startCharInStaticText = i === inlineTextNodeIndex ?
          ParagraphUtils.getStartCharIndexInParent(inlineTextNode) :
          startCharInStaticText - inlineTextNode.name.length;

      // Continue to the next one if the start of this inlineTextNode is still
      // bigger than startCharIndex.
      if (startCharInStaticText + staticTextStartChar >= startCharIndex) {
        continue;
      }

      // Iterate over all sentenceStarts to find the one that is smaller than
      // startCharIndex.
      for (let j = inlineTextNode.sentenceStarts.length - 1; j >= 0; j--) {
        const potentialStart = inlineTextNode.sentenceStarts[j] +
            startCharInStaticText + staticTextStartChar;
        if (potentialStart >= startCharIndex) {
          continue;
        }
        return potentialStart;
      }
    }
    // We are off the edge of this static text node, return not found.
    return null;
  }
}