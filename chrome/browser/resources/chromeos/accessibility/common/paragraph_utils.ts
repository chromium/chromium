// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from './testing/test_import_manager.js';
import {WordUtils} from './word_utils.js';

type AutomationNode = chrome.automation.AutomationNode;
const RoleType = chrome.automation.RoleType;

interface NodeGroupBuildOptions {
  splitOnLanguage?: boolean;
  splitOnParagraph?: boolean;
  clipOverflowWords?: boolean;
}

interface NodeGroupWithOffset {
  nodeGroup: ParagraphUtils.NodeGroup;
  startIndexInGroup?: number;
  endIndexInGroup?: number;
}

type NodeGroupByCharIndex = {
  node: AutomationNode,
  offset: number,
}|{node: null, offset: 0};

export class ParagraphUtils {
  /**
   * @return Whether the given node is a paragraph.
   * TODO(joelriley@google.com): Consider expanding what is considered a block,
   * for instance, any non-inline node.
   */
  static isBlock(node: AutomationNode): boolean {
    if (node.role === RoleType.PARAGRAPH || node.role === RoleType.SVG_ROOT) {
      return true;
    }
    if (node.display !== undefined && node.display !== 'inline' &&
        node.role !== RoleType.STATIC_TEXT &&
        (node.parent && node.parent.role !== RoleType.SVG_ROOT)) {
      return true;
    }
    // Many native view containers have UNKNOWN roles. Web elements will
    // probably not have UNKNOWN roles. This allows us to break at native
    // view containers rather than walking up to the Desktop root.
    if (node.role === RoleType.UNKNOWN) {
      return true;
    }
    // For Android views. Clickable is Android-only property, and clickable is
    // usually a small meaningful chunk.
    if (node.clickable) {
      return true;
    }
    return false;
  }

  /**
   * TODO(b/314204374): Should Return undefined instead of null.
   * Gets the first ancestor of a node which is a paragraph or is not inline,
   * or get the root node if none is found.
   * @param node The node to get the parent for.
   * @return the parent paragraph or null if there is none.
   */
  static getFirstBlockAncestor(node: AutomationNode): AutomationNode|null {
    let parent = node.parent;
    const root = node.root;
    while (parent) {
      if (parent === root || ParagraphUtils.isBlock(parent)) {
        return parent;
      }
      parent = parent.parent;
    }
    return null;
  }

  /**
   * Determines whether two nodes are in the same block-like ancestor, i.e.
   * whether they are in the same paragraph.
   * @param first The first node to compare.
   * @param second The second node to compare.
   * @return whether two nodes are in the same paragraph.
   */
  static inSameParagraph(first?: AutomationNode, second?: AutomationNode):
      boolean {
    if (first === undefined || second === undefined) {
      return false;
    }
    // TODO: Clean up this check after crbug.com/774308 is resolved.
    // At that point we will only need to check for display:block or
    // inline-block.
    if (((first.display === 'block' || first.display === 'inline-block') &&
         first.role !== RoleType.STATIC_TEXT &&
         first.role !== RoleType.INLINE_TEXT_BOX) ||
        ((second.display === 'block' || second.display === 'inline-block') &&
         second.role !== RoleType.STATIC_TEXT &&
         second.role !== RoleType.INLINE_TEXT_BOX)) {
      // 'block' or 'inline-block' elements cannot be in the same paragraph.
      return false;
    }
    const firstBlock = ParagraphUtils.getFirstBlockAncestor(first);
    const secondBlock = ParagraphUtils.getFirstBlockAncestor(second);
    return firstBlock !== undefined && firstBlock === secondBlock;
  }

  /**
   * Determines whether a string is only whitespace.
   * @param name A string to test
   * @return whether the string is only whitespace
   */
  static isWhitespace(name?: string): boolean {
    if (name === undefined || name.length === 0) {
      return true;
    }
    // Search for one or more whitespace characters
    const re = /^\s+$/;
    return re.exec(name) != null;
  }

  /**
   * Gets the text to be read aloud for a particular node.
   * @return The text to read for this node.
   */
  static getNodeName(node: AutomationNode): string {
    if (node.role === RoleType.TEXT_FIELD &&
        (node.children === undefined || node.children.length === 0) &&
        node.value) {
      // A text field with no children should use its value instead of
      // the name element, this is the contents of the text field.
      // This occurs in native UI such as the omnibox.
      return node.value;
    } else if (
        node.role === RoleType.CHECK_BOX ||
        node.role === RoleType.MENU_ITEM_CHECK_BOX) {
      let stateString: string;
      if (node.checked === 'true') {
        stateString =
            chrome.i18n.getMessage('select_to_speak_checkbox_checked');
      } else if (node.checked === 'mixed') {
        stateString = chrome.i18n.getMessage('select_to_speak_checkbox_mixed');
      } else {
        stateString =
            chrome.i18n.getMessage('select_to_speak_checkbox_unchecked');
      }

      return !ParagraphUtils.isWhitespace(node.name) ?
          node.name + ' ' + stateString :
          stateString;
    } else if (
        node.role === RoleType.RADIO_BUTTON ||
        node.role === RoleType.MENU_ITEM_RADIO) {
      const stateString = chrome.i18n.getMessage(
          'select_to_speak_radiobutton_' +
          (node.checked === 'true' ?
               'selected' :
               (node.checked === 'mixed' ? 'mixed' : 'unselected')));
      return !ParagraphUtils.isWhitespace(node.name) ?
          node.name + ' ' + stateString :
          stateString;
    }
    return node.name ? node.name : '';
  }

  /**
   * Gets the text to be read aloud for a particular node.
   * Compared with the bounds of the blockParent, the overflow
   * words of the text will be replaced with empty space.
   * @return The text to read for this node.
   */
  static getNodeNameWithoutOverflowWords(
      nodeGroupItem: ParagraphUtils.NodeGroupItem,
      blockParent?: AutomationNode): string {
    const unclippedText = ParagraphUtils.getNodeName(nodeGroupItem.node);
    if (!blockParent || blockParent.location == null) {
      return unclippedText;
    }

    // Get the bounds of blockparent.
    const bounds = blockParent.location;
    const leftBound = bounds.left;
    const rightBound = bounds.left + bounds.width;
    const topBound = bounds.top;
    const bottomBound = bounds.top + bounds.height;

    const nodeBounds = nodeGroupItem.node.unclippedLocation;
    // TODO(b/314203187): `!` operator used here since node bounds was
    // previously infered to be not undefined.
    const nodeLeftBound = nodeBounds!.left;
    const nodeRightBound = nodeBounds!.left + nodeBounds!.width;
    const nodeTopBound = nodeBounds!.top;
    const nodeBottomBound = nodeBounds!.top + nodeBounds!.height;

    // If the node bounds are entirely within the blockparent bounds, return
    // the unclipped text.
    if (nodeLeftBound >= leftBound && nodeRightBound <= rightBound &&
        nodeTopBound >= topBound && nodeBottomBound <= bottomBound) {
      return unclippedText;
    }

    // If the node bounds are entirely out of the blockparent bounds, return
    // empty text with the same length of the unclipped text.
    if (nodeLeftBound >= rightBound || nodeRightBound <= leftBound ||
        nodeTopBound >= nodeBottomBound || nodeBottomBound <= topBound) {
      return ' '.repeat(unclippedText.length);
    }

    // Go through words one by one and construct the output text.
    let outputText = unclippedText;
    let index = 0;
    while (index < unclippedText.length) {
      // The index of the first char of the word. The startIndex is guaranteed
      // to be equal or lager than the input index, and is capped at
      // nodeGroupItem.name.length.
      const startIndex = WordUtils.getNextWordStart(
          unclippedText,
          index,
          nodeGroupItem,
          true /* ignoreStartChar */,
      );

      // The index of the last char of the word + 1. The endIndex is guaranteed
      // to be larger than the input startIndex, and is capped at
      // unclippedText.length.
      const endIndex = WordUtils.getNextWordEnd(
          unclippedText,
          startIndex,
          nodeGroupItem,
          true /* ignoreStartChar */,
      );

      // Prepare the index for the next word. The endIndex is guarantted to be
      // larger than the original index, and is capped at unclippedText.length,
      // so the while loop will be stopped.
      index = endIndex;

      // If nodeGroupItem.hasInlineText is true, the nodeGroupItem is a
      // staticText node that has inlineTextBox children, and we need to select
      // a child inlineTextBox node corresponding to the startIndex. We also
      // need to offset the query indexes for the inlineTextBox node. If
      // nodeGroupItem.hasInlineText is false, the node of the NodeGroupItem
      // should not be an inlineTextBox node, and we assigns the node and
      // indexes directly from the nodeGroupItem.
      let node: AutomationNode;
      let boundQueryStartIndex;
      let boundQueryEndIndex;
      if (nodeGroupItem.hasInlineText) {
        // TODO(b/314203187): Not null asserted, an investigation to ensure the
        // function doesn't return undefined is needed.
        node = ParagraphUtils.findInlineTextNodeByCharacterIndex(
            nodeGroupItem.node, startIndex)!;
        const charIndexInParent =
            ParagraphUtils.getStartCharIndexInParent(node);
        boundQueryStartIndex = startIndex - charIndexInParent;
        boundQueryEndIndex = endIndex - charIndexInParent;
      } else {
        console.assert(
            nodeGroupItem.node.role !== RoleType.INLINE_TEXT_BOX,
            'NodeGroupItem.node should not be an inlineTextBox node');
        node = nodeGroupItem.node;
        boundQueryStartIndex = startIndex;
        boundQueryEndIndex = endIndex;
      }

      node!.unclippedBoundsForRange(
          boundQueryStartIndex, boundQueryEndIndex, b => {
            // If the word is entirely out of the blockparent bounds,
            // replace the word with space characters.
            if (b.left + b.width <= leftBound || b.left >= rightBound ||
                b.top >= bottomBound || b.top + b.height <= topBound) {
              outputText = outputText.substr(0, startIndex) +
                  ' '.repeat(endIndex - startIndex) +
                  outputText.substr(endIndex);
            }
          });
    }
    return outputText;
  }

  /**
   * Determines the index into the parent name at which the inlineTextBox
   * node name begins.
   * TODO(leileilei@google.com): Corrects the annotation of |inlineTextNode|
   * to non-nullable.
   * @param inlineTextNode An inlineTextBox type node.
   * @return The character index into the parent node at which
   *     this node begins.
   */
  static getStartCharIndexInParent(inlineTextNode: AutomationNode): number {
    let result = 0;
    // TODO(b/314203187): Not null asserted, check these to make sure this is
    // correct.
    for (let i = 0; i < inlineTextNode.indexInParent!; i++) {
      result += inlineTextNode.parent!.children[i]!.name!.length;
    }
    return result;
  }

  /**
   * Determines the inlineTextBox child of a staticText node that appears
   * at the given character index into the name of the staticText node. See the
   * |findInlineTextNodeIndexByCharacterIndex| function below.
   * @param staticTextNode The staticText node to search.
   * @param index The index into the staticTextNode's name.
   * @return The inlineTextBox node within the staticText
   *    node that appears at this index into the staticText node's name, or
   *    the last inlineTextBox in the staticText node if the index is too
   *    large.
   */
  static findInlineTextNodeByCharacterIndex(
      staticTextNode: AutomationNode, index: number): AutomationNode|null {
    const inlineTextNodeIndex =
        ParagraphUtils.findInlineTextNodeIndexByCharacterIndex(
            staticTextNode, index);
    if (inlineTextNodeIndex < 0) {
      // TODO(b/314204374): Return undefined instead.
      return null;
    }
    return staticTextNode.children[inlineTextNodeIndex];
  }

  /**
   * Determines the inlineTextBox child of a staticText node that appears
   * at the given character index into the name of the staticText node. Uses
   * the inlineTextBox's name length to determine position. For example, if
   * a staticText has name "abc 123" and two children with names "abc " and
   * "123", indexes 0-3 would return the index of the first child (i.e., 0)
   * and indexes 4+ would return the index of the second child (i.e., 1).
   * @param staticTextNode The staticText node to search.
   * @param index The index into the staticTextNode's name.
   * @return The index of the inlineTextBox node within the
   *    staticText node that appears at the staticTextNode's name index into
   *    the staticText node's name, or the last inlineTextBox index in the
   *    staticText node if the staticTextNode's name index is too large. Return
   *    a negative number (-1) if the staticTextNode has no children.
   */
  static findInlineTextNodeIndexByCharacterIndex(
      staticTextNode: AutomationNode, index: number): number {
    if (staticTextNode.children.length === 0) {
      return -1;
    }
    let textLength = 0;
    for (let i = 0; i < staticTextNode.children.length; i++) {
      const node = staticTextNode.children[i];
      // TODO(b/314203187): Not null asserted, check these to make sure this is
      // correct.
      if (node!.name!.length + textLength > index) {
        return i;
      }
      // TODO(b/314203187): Not null asserted, check these to make sure this is
      // correct.
      textLength += node!.name!.length;
    }
    return staticTextNode.children.length - 1;
  }

  /**
   * Builds information about nodes in a group until it reaches the end of the
   * group. It may return a NodeGroup with a single node, or a large group
   * representing a paragraph of inline nodes.
   * @param nodes List of automation nodes to use.
   * @param index The index into nodes at which to start.
   * @param options
   *     splitOnLanguage: flag to determine if we should split nodes up based on
   * language. If this is not passed, default to false.
   *     splitOnParagraph: flag to determine if we should split nodes up based
   * on paragraph. If this is not passed, default to true.
   *     clipOverflowWords: Whether to clip generated text.
   * @return info about the node group
   */
  static buildNodeGroup(
      nodes: AutomationNode[], index: number,
      options?: NodeGroupBuildOptions): ParagraphUtils.NodeGroup {
    options = options || {};
    const splitOnLanguage = options.splitOnLanguage || false;
    const splitOnParagraph = options.splitOnParagraph === undefined ?
        true :
        options.splitOnParagraph;
    const clipOverflowWords = options.clipOverflowWords || false;
    let node = nodes[index];
    let next = nodes[index + 1];

    const blockParent = node!.clickable ?
        node :
        // TODO(b/314204374): The function returns null, but we aren't using
        // that here. It will need to be cleaned up later.
        (ParagraphUtils.getFirstBlockAncestor(nodes[index]!) ?? undefined);
    const result = new ParagraphUtils.NodeGroup(blockParent);
    let staticTextParent: ParagraphUtils.NodeGroupItem|undefined;
    let currentLanguage = undefined;
    // TODO: Don't skip nodes. Instead, go through every node in
    // this paragraph from the first to the last in the nodes list.
    // This will catch nodes at the edges of the user's selection like
    // short links at the beginning or ends of sentences.
    //
    // While next node is in the same paragraph as this node AND is
    // a text type node, continue building the paragraph.
    while (index < nodes.length) {
      const name = ParagraphUtils.getNodeName(node);
      if (!ParagraphUtils.isWhitespace(name)) {
        let newNode;
        if (node.role === RoleType.INLINE_TEXT_BOX &&
            node.parent !== undefined) {
          if (node.parent.role === RoleType.STATIC_TEXT) {
            // This is an inlineTextBox node with a staticText parent. If that
            // parent is already added to the result, we can skip. This adds
            // each parent only exactly once.
            if (staticTextParent && staticTextParent.node !== node.parent) {
              // We are on a new staticText. Make a new parent to add to.
              staticTextParent = undefined;
            }
            if (!staticTextParent) {
              staticTextParent = new ParagraphUtils.NodeGroupItem(
                  node.parent, result.text.length, true);
              newNode = staticTextParent;
            }
          } else {
            // Not an staticText parent node. Add it directly.
            newNode = new ParagraphUtils.NodeGroupItem(
                node, result.text.length, false);
          }
        } else {
          // Not an inlineTextBox node. Add it directly, as each node in this
          // list is relevant.
          newNode =
              new ParagraphUtils.NodeGroupItem(node, result.text.length, false);
        }
        if (newNode) {
          if (clipOverflowWords) {
            result.text += ParagraphUtils.getNodeNameWithoutOverflowWords(
                newNode, blockParent);
          } else {
            result.text += ParagraphUtils.getNodeName(newNode.node);
          }
          // Add space between each node.
          result.text += ' ';
          result.nodes.push(newNode);
        }
      }

      // Set currentLanguage if we don't have one yet.
      // We have to do this before we consider stopping otherwise we miss out on
      // the last language attribute of each NodeGroup which could be important
      // if this NodeGroup only contains a single node, or if all previous nodes
      // lacked any language information.
      if (!currentLanguage) {
        currentLanguage = node.detectedLanguage;
      }

      // Stop if any of following is true:
      //  1. we have no more nodes to process.
      //  2. we need to check for a paragraph split and if the next node is not
      //  part of the same paragraph.
      //  3. The next node is clickable, which can be considered as a paragraph
      //  in Android.
      if (index + 1 >= nodes.length ||
          (splitOnParagraph && !ParagraphUtils.inSameParagraph(node, next)) ||
          next.clickable) {
        break;
      }

      // Stop if the next node would change our currentLanguage (if we have
      // one). We allow an undefined detectedLanguage to match with any previous
      // language, so that we will never break up a NodeGroup on an undefined
      // detectedLanguage.
      if (splitOnLanguage && currentLanguage && next.detectedLanguage &&
          currentLanguage !== next.detectedLanguage) {
        break;
      }

      index += 1;
      node = next;
      next = nodes[index + 1];
    }

    if (splitOnLanguage && currentLanguage) {
      result.detectedLanguage = currentLanguage;
    }
    result.endIndex = index;
    return result;
  }

  /**
   * Builds a single node group that contains all the input |nodes|. In
   * addition, this function will transform the provided node offsets to values
   * that are relative to the text of the resulting node group. This function
   * can be used to check the sentence boundaries across all the input nodes.
   * @param nodes The nodes for the selected content.
   * @param startIndex The index into the first node's text at which the
   *     selected content starts.
   * @param endIndex The index into the last node's text at which the selected
   *     content ends.
   * @return
   *     nodeGroup: The node group that contains all the input |nodes|. The node
   * group will not consider any language difference or paragraph split.
   *     startOffsetInGroup: The index into the node group's text at which the
   * selected content starts.
   *     endOffsetInGroup: The index into the node group's text at which the
   * selected content ends.
   */
  static buildSingleNodeGroupWithOffset(
      nodes: AutomationNode[], startIndex?: number,
      endIndex?: number): NodeGroupWithOffset {
    const nodeGroup = ParagraphUtils.buildNodeGroup(
        nodes, 0 /* index */,
        {splitOnLanguage: false, splitOnParagraph: false});
    if (startIndex !== undefined) {
      // The first node of the NodeGroup may not be at the beginning of the
      // parent of the NodeGroup. (e.g., an inlineText in its staticText
      // parent). Thus, we need to adjust the |opt_startIndex|.
      const firstNodeHasInlineText =
          nodeGroup.nodes.length > 0 && nodeGroup.nodes[0].hasInlineText;
      const startIndexInNodeParent = firstNodeHasInlineText ?
          ParagraphUtils.getStartCharIndexInParent(nodes[0]) :
          0;
      startIndex += startIndexInNodeParent + nodeGroup.nodes[0].startChar;
    }
    if (endIndex !== undefined) {
      // Similarly, |opt_endIndex| needs to be adjusted.
      const lastNodeHasInlineText = nodeGroup.nodes.length > 0 &&
          nodeGroup.nodes[nodeGroup.nodes.length - 1].hasInlineText;
      const startIndexInNodeParent = lastNodeHasInlineText ?
          ParagraphUtils.getStartCharIndexInParent(nodes[0]) :
          0;
      endIndex += startIndexInNodeParent +
          nodeGroup.nodes[nodeGroup.nodes.length - 1].startChar;
    }

    return {
      nodeGroup,
      startIndexInGroup: startIndex,
      endIndexInGroup: endIndex,
    };
  }

  /**
   * Finds the AutomationNode that appears at the given character index within
   * the |nodeGroup|.
   * @param nodeGroup The nodeGroup that has the nodeGroupItem.
   * @param charIndex The char index into the nodeGroup's text. The index is
   *     relative to the start of the |nodeGroup|.
   * @return
   *     node: the AutomationNode within the |nodeGroup| that appears at
   * |charIndex|. For a static text node that has inline text nodes, we will
   * return the inline text node corresponding to the |charIndex|.
   *     offset: the offset indicating the position of the |charIndex| within
   * the found nodeGroupItem. The offset is relative to the start of the |node|.
   */
  static findNodeFromNodeGroupByCharIndex(
      nodeGroup: ParagraphUtils.NodeGroup,
      charIndex: number): NodeGroupByCharIndex {
    for (const currentNodeGroupItem of nodeGroup.nodes) {
      const currentNode = currentNodeGroupItem.node;
      const currentNodeNameLength =
          ParagraphUtils.getNodeName(currentNode).length;
      // We iterate over each nodeGroupItem until the |charIndex| is pointing
      // before the current node's name.
      if (currentNodeGroupItem.startChar + currentNodeNameLength > charIndex) {
        // The |currentNodeOffset| is the position of the |charIndex| within the
        // current nodeGroupItem. ParagraphUtils.getNodeName returns a string
        // without an ending space character, though |charIndex| is based on a
        // string that was generated with an extra space character between each
        // nodeGroupItems. Thus, there might be a gap between the previous
        // nodeGroupItem and the current nodeGroupItem. If
        // |currentNodeGroupItem.startChar| is greater than |charIndex|, that
        // means the |charIndex| is pointing to the gap. In such case, we set
        // |currentNodeOffset| to 0 and return the beginning of the current
        // nodeGroupItem.
        const currentNodeOffset =
            Math.max(0, charIndex - currentNodeGroupItem.startChar);
        if (!currentNodeGroupItem.hasInlineText) {
          // If the nodeGroupItem does not have inline text, we return the
          // corresponding node and the current offset.
          return {node: currentNode, offset: currentNodeOffset};
        }

        const inlineTextNode =
            ParagraphUtils.findInlineTextNodeByCharacterIndex(
                currentNode, currentNodeOffset);

        return inlineTextNode ? {
          node: inlineTextNode,
          offset: currentNodeOffset -
              ParagraphUtils.getStartCharIndexInParent(inlineTextNode),
        } :
                                {node: currentNode, offset: currentNodeOffset};
      }
    }
    return {node: null, offset: 0};
  }
}

export namespace ParagraphUtils {

  /**
   * Class representing a node group, which may be a single node or a
   * full paragraph of nodes.
   */
  export class NodeGroup {
    /**
     * Full text of this paragraph.
     */
    text = '';
    /**
     * List of nodes in this paragraph in order.
     */
    nodes: ParagraphUtils.NodeGroupItem[] = [];

    /**
     * The block parent of this NodeGroup, if there is one.
     */
    blockParent?: AutomationNode;

    /**
     * The index of the last node in this paragraph from the list of
     * nodes originally selected by the user.
     * Note that this may not be stable over time, because nodes may
     * come and go from the automation tree. This should not be used
     * in any callbacks / asynchronously.
     */
    endIndex = -1;

    /**
     * Language and country code for all nodes within this NodeGroup.
     */
    detectedLanguage?: string;

    /**
     * The offset marks the end index of selected content in this nodeGroup. For
     * example, if a user selects a part of a paragraph, we will remove all text
     * after the |endOffset| so it is not spoken.
     */
    endOffset?: number;

    /**
     * @param blockParent The first block ancestor of this group. This may be
     *     the paragraph parent, for example.
     */
    constructor(blockParent?: AutomationNode) {
      this.blockParent = blockParent;
    }
  }

  /**
   * Class representing an automation node within a block of text, like
   * a paragraph. Each Item in a NodeGroup has a start index within the
   * total text, as well as the original AutomationNode it was associated
   * with.
   */
  export class NodeGroupItem {
    node: AutomationNode;
    /**
     * The index into the NodeGroup's text string that is the first character
     * of the text of this automation node.
     */
    startChar: number;
    /**
     * If this is a staticText node which has inlineTextBox children which
     * should be selected. We cannot select the inlineTextBox children directly
     * because they are not guaranteed to be stable.
     */
    hasInlineText: boolean;

    /**
     * @param node The AutomationNode associated with this item
     * @param startChar The index into the NodeGroup's text string where
     *     this item begins.
     * @param hasInlineText If this NodeGroupItem has inlineText
     *     children.
     */
    constructor(
        node: AutomationNode, startChar: number, hasInlineText?: boolean) {
      this.node = node;
      this.startChar = startChar;


      this.hasInlineText = hasInlineText !== undefined ? hasInlineText : false;
    }
  }

}

TestImportManager.exportForTesting(ParagraphUtils);
