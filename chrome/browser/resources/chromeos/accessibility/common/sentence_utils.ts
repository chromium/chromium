// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {constants} from './constants.js';
import {ParagraphUtils} from './paragraph_utils.js';

const RoleType = chrome.automation.RoleType;

/**
 * Utilities for processing sentences within strings and node groups.
 */
export class SentenceUtils {
  /**
   * Gets the sentence start from the current position. When |direction| is set
   * to forward, this function will incrementally go over each nodeGroupItem in
   * |nodeGroup|, and try to find a sentence start index after the
   * |startCharIndex|. When |direction| is set to backward, This function will
   * search for the sentence start before the current position. The
   * |startCharIndex| and the found sentence start index are relative to the
   * text content of this node group.
   * @param nodeGroup The node group this function
   *     will search.
   * @param startCharIndex The char index that we start from. This
   *     index is relative to the text content of this node group and is
   *     exclusive: if a sentence start at 0 and we search with a 0
   *     |startCharIndex|, this function will return the next sentence start
   *     after 0 if we search forward.
   * @param direction Direction to search for the next sentence
   *     start. |constants.Dir.BACKWARD| will search for the sentence start
   *     before the current position. |constants.Dir.FORWARD| will search for
   *     the sentence start after the current position.
   * @return The next sentence start after |startCharIndex|, returns
   *     null if nothing found.
   */
  static getSentenceStart(
      nodeGroup: ParagraphUtils.NodeGroup, startCharIndex: number,
      direction: constants.Dir): number|null {
    if (!nodeGroup) {
      return null;
    }
    if (nodeGroup.nodes.length === 0) {
      return null;
    }

    if (direction === constants.Dir.FORWARD) {
      for (let i = 0; i < nodeGroup.nodes.length; i++) {
        const nodeGroupItem = nodeGroup.nodes[i];
        const result = SentenceUtils.getSentenceStartInNodeGroupItem(
            nodeGroupItem, startCharIndex, constants.Dir.FORWARD);
        if (result !== null) {
          return result;
        }
      }
    } else if (direction === constants.Dir.BACKWARD) {
      for (let i = nodeGroup.nodes.length - 1; i >= 0; i--) {
        const nodeGroupItem = nodeGroup.nodes[i];
        const result = SentenceUtils.getSentenceStartInNodeGroupItem(
            nodeGroupItem, startCharIndex, constants.Dir.BACKWARD);
        if (result !== null) {
          return result;
        }
      }
    }
    return null;
  }

  /**
   * Gets the sentence start before or after current position within the input
   * |nodeGroupItem|.
   * @param nodeGroupItem The node group item
   *     this function will search.
   * @param startCharIndex The char index that we start from. This is
   *     relative to the text content of the node group.
   * @param direction Direction to search for the next sentence
   *     start. |constants.Dir.BACKWARD| will search for the sentence start
   *     before the current position. |constants.Dir.FORWARD| will search for
   *     the sentence start after the current position.
   * @return The next sentence start after |startCharIndex|, returns
   *     null if nothing found.
   */
  static getSentenceStartInNodeGroupItem(
      nodeGroupItem: ParagraphUtils.NodeGroupItem, startCharIndex: number,
      direction: constants.Dir): number|null {
    if (!nodeGroupItem) {
      return null;
    }
    // Check if this nodeGroupItem has a non-empty static text node.
    if (nodeGroupItem.node.role !== RoleType.STATIC_TEXT ||
        nodeGroupItem.node.name!.length === 0) {
      return null;
    }

    const staticTextStartChar = nodeGroupItem.startChar;
    const staticTextNode = nodeGroupItem.node;

    if (direction === constants.Dir.FORWARD) {
      // If the corresponding char of |startCharIndex| is after this static text
      // node, skip this text node.
      if (startCharIndex >
          staticTextStartChar + staticTextNode.name!.length - 1) {
        return null;
      }
      // If the corresponding char of |startCharIndex| is within this static
      // text node, we get its relative index in this static text. Otherwise,
      // the start char is before this static text node, and any sentence start
      // in this static text node is valid.
      const searchIndexInStaticText =
          Math.max(startCharIndex - staticTextStartChar, -1);

      // Iterate over all sentenceStarts to find the one that is bigger than
      // |searchIndexInStaticText|.
      for (let i = 0; i < staticTextNode.sentenceStarts!.length; i++) {
        if (staticTextNode.sentenceStarts![i] <= searchIndexInStaticText) {
          continue;
        }
        return staticTextNode.sentenceStarts![i] + staticTextStartChar;
      }
    } else if (direction === constants.Dir.BACKWARD) {
      // If the corresponding char of |startCharIndex| is before this static
      // text node, skip this text node.
      if (startCharIndex < staticTextStartChar) {
        return null;
      }
      // If the corresponding char of |startCharIndex| is within this static
      // text node, we get its relative index in this static text. Otherwise,
      // the start char is after this static text node, and any sentence start
      // in this static text node is valid.
      const searchIndexInStaticText = Math.min(
          startCharIndex - staticTextStartChar, staticTextNode.name!.length);

      // Iterate over all sentenceStarts to find the one that is smaller than
      // |searchIndexInStaticText|.
      for (let i = staticTextNode.sentenceStarts!.length - 1; i >= 0; i--) {
        if (staticTextNode.sentenceStarts![i] >= searchIndexInStaticText) {
          continue;
        }
        return staticTextNode.sentenceStarts![i] + staticTextStartChar;
      }
    }
    // We are off the edge of this static text node, return null.
    return null;
  }

  /**
   * Checks if the current position is a sentence start.
   * @param nodeGroup The node group of the current
   *     position.
   * @param currentCharIndex The char index of the current position.
   *     This is relative to the text content of the node group.
   * @return Whether the current position is a start of a sentence.
   */
  static isSentenceStart(
      nodeGroup: ParagraphUtils.NodeGroup, currentCharIndex: number): boolean {
    if (!nodeGroup) {
      return false;
    }

    if (nodeGroup.nodes.length === 0) {
      return false;
    }

    // Iterate over all the nodeGroupItems.
    for (let i = 0; i < nodeGroup.nodes.length; i++) {
      const nodeGroupItem = nodeGroup.nodes[i];
      // Check if this nodeGroupItem has a non-empty static text node.
      if (nodeGroupItem.node.role !== RoleType.STATIC_TEXT ||
          nodeGroupItem.node.name!.length === 0) {
        continue;
      }

      // Check if the corresponding char of |currentCharIndex| is inside this
      // static text node.
      const staticTextStartChar = nodeGroupItem.startChar;
      const staticTextNode = nodeGroupItem.node;
      if (currentCharIndex >
              staticTextStartChar + staticTextNode.name!.length - 1 ||
          currentCharIndex < staticTextStartChar) {
        continue;
      }

      const searchIndexInStaticText = currentCharIndex - staticTextStartChar;

      // Iterate over all sentenceStarts in the staticTextNode to see if we
      // have |searchIndexInStaticText|.
      for (let j = 0; j < staticTextNode.sentenceStarts!.length; j++) {
        if (staticTextNode.sentenceStarts![j] === searchIndexInStaticText) {
          return true;
        }
      }
    }

    // We did not find a sentence start equal to |currentCharIndex|.
    return false;
  }
}
