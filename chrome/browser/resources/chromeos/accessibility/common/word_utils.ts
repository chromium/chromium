// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ParagraphUtils} from './paragraph_utils.js';

// Utilities for processing words within strings and nodes.

export class WordUtils {
  /**
   * Searches through text starting at an index to find the next word's
   * start boundary.
   * @param text The string to search through
   * @param indexAfter The index into text at which to start
   *      searching.
   * @param nodeGroupItem The node whose name we
   *      are searching through.
   * @param ignoreStartChar When set to true, the search will only
   *      consider the index within the input node and ignore
   *      nodeGroupItem.startChar offsets. This is useful when we only search
   *      within the input nodeGroupItem, instead of the parent nodeGroup.
   * @return The index of the next word's start
   */
  static getNextWordStart(
      text: string|undefined, indexAfter: number,
      nodeGroupItem: ParagraphUtils.NodeGroupItem,
      ignoreStartChar = false): number {
    if (nodeGroupItem.hasInlineText && nodeGroupItem.node.children.length > 0) {
      const startChar = ignoreStartChar ? 0 : nodeGroupItem.startChar;
      // TODO(b/314203187): Not null asserted, check these to make sure this is
      // correct.
      const node = ParagraphUtils.findInlineTextNodeByCharacterIndex(
          nodeGroupItem.node, indexAfter - startChar)!;
      const startCharInParent = ParagraphUtils.getStartCharIndexInParent(node);
      // TODO(b/314203187): Not nulls asserted, check these to make sure they
      // are correct.
      for (var i = 0; i < node.wordStarts!.length; i++) {
        if (node.wordStarts![i] + startChar + startCharInParent < indexAfter) {
          continue;
        }
        return node.wordStarts![i] + startChar + startCharInParent;
      }
      // Default: We are just off the edge of this node.
      return node.name!.length + startChar + startCharInParent;
    } else {
      // Try to parse using a regex, which is imperfect.
      // Fall back to the given index if we can't find a match.
      return WordUtils.nextWordHelper(
          text, indexAfter, WordUtils.WORD_START_REGEXP, indexAfter);
    }
  }

  /**
   * Searches through text starting at an index to find the next word's
   * end boundary.
   * @param text The string to search through
   * @param indexAfter The index into text at which to start
   *      searching.
   * @param nodeGroupItem The node whose name we
   *      are searching through.
   * @param ignoreStartChar When set to true, the search will only
   *      consider the index within the input node and ignore
   *      nodeGroupItem.startChar offsets. This is useful when we only search
   *      within the input nodeGroupItem, instead of the parent nodeGroup.
   * @return The index of the next word's end
   */
  static getNextWordEnd(
      text: string|undefined, indexAfter: number,
      nodeGroupItem: ParagraphUtils.NodeGroupItem,
      ignoreStartChar = false): number {
    if (nodeGroupItem.hasInlineText && nodeGroupItem.node.children.length > 0) {
      const startChar = ignoreStartChar ? 0 : nodeGroupItem.startChar;
      // TODO(b/314203187): Not null asserted, check these to make sure this is
      // correct.
      const node = ParagraphUtils.findInlineTextNodeByCharacterIndex(
          nodeGroupItem.node, indexAfter - startChar + 1)!;
      const startCharInParent = ParagraphUtils.getStartCharIndexInParent(node);
      // TODO(b/314203187): Not nulls asserted, check these to make sure they
      // are correct.
      for (var i = 0; i < node.wordEnds!.length; i++) {
        if (node.wordEnds![i] + startChar + startCharInParent - 1 <
            indexAfter) {
          continue;
        }
        const result = node.wordEnds![i] + startChar + startCharInParent;
        // TODO(b/314203187): Not null asserted, check these to make sure this
        // is correct.
        return text!.length > result ? result : text!.length;
      }
      // Default.
      // TODO(b/314203187): Not null asserted, check these to make sure this is
      // correct.
      return text!.length;
    } else {
      // Try to parse using a regex, which is imperfect.
      // Fall back to the full length of the text if we can't find a match.
      // TODO(b/314203187): Not null asserted, check these to make sure this is
      // correct.
      return WordUtils.nextWordHelper(
                 text, indexAfter, WordUtils.WORD_END_REGEXP,
                 text!.length - 1) +
          1;
    }
  }

  /**
   * Searches through text to find the first index of a regular expression
   * after a given starting index. Returns a default value if no match is
   * found.
   * @param text The string to search through
   * @param indexAfter The index at which to start searching
   * @param re A regular expression to search for
   * @param defaultValue The default value to return if no
                       match is found.
   * @return The index found by the regular expression, or -1
   *                    if none found.
   */
  static nextWordHelper(
      text: string|undefined, indexAfter: number, re: RegExp,
      defaultValue: number): number {
    if (text === undefined) {
      return defaultValue;
    }
    const result = re.exec(text.substr(indexAfter));
    if (result != null && result.length > 0) {
      return indexAfter + result.index;
    }
    return defaultValue;
  }
}

export namespace WordUtils {

  /**
   * Regular expression to find the start of the next word after a word
   * boundary. We cannot use \b\W to find the next word because it does not
   * match many unicode characters.
   */
  export const WORD_START_REGEXP: RegExp = /\b\S/;

  /**
   * Regular expression to find the end of the next word, which is followed by
   * whitespace. We cannot use \w\b to find the end of the previous word because
   * \w does not know about many unicode characters.
   */
  export const WORD_END_REGEXP: RegExp = /\S\s/;
}
