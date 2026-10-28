// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isBlockTrackingEnabled} from '../shared/common.js';
import {getFirstTextNodeAtOrAfter, getLastTextNodeBefore, isBackwardSelection} from '../shared/dom_queries.js';

import type {BlockRange} from './node_store.js';
import {NodeStore} from './node_store.js';
import type {ReadingModeSelection, SelectionWithIds} from './read_anything_types.js';

// A position in the DOM, as in a Range or Selection endpoint. For a text node,
// the offset is a character offset into its text. For an element, the offset is
// a child index: the point is before the child at that index, or after the last
// child if the offset equals the number of children.
interface BoundaryPoint {
  node: Node;
  offset: number;
}

// A text node with selectable text, and the AXNodeID of the innermost block
// that contains it. The searches for the first and last selected text return
// both: the text nodes are needed to check that text between them is selected,
// and the block IDs are found while checking that the text is selectable, so
// returning them avoids finding them again (see getEnclosingBlockId_).
interface SelectableText {
  textNode: Text;
  blockId: number;
}

// Maps selections between the reading mode panel and the main panel one block
// (e.g. paragraph or heading) at a time, for when the reading mode content is
// translated.
//
// Translation rewrites the text inside the reading mode panel, so its text
// nodes are no longer the ones in the NodeStore, and its text doesn't
// correspond word for word to the text in the main panel. However, translation
// translates each block as a unit, and it doesn't replace the block elements
// themselves. So a selection on one side is mapped to the blocks that contain
// it, and then to all of the text in those blocks on the other side. See
// NodeStore.registerTextInBlock.
//
// Selections of whole blocks in the reading mode panel have their endpoints in
// the blocks' parents rather than inside the blocks, so that they stay valid
// while translation rewrites the blocks' contents. So the anchor is the
// parent of the first block, and the first block is the anchor's child at the
// anchor offset.
//
// The mapper is only used for TranslatePdf, so its methods do nothing when the
// feature is disabled or the current page isn't a PDF.
export class TranslatedSelectionMapper {
  private nodeStore_: NodeStore = NodeStore.getInstance();

  // Returns the AX selection of all of the text in the blocks that contain the
  // text selected in the reading mode panel, i.e. the selection that would be
  // made by selecting those blocks in the untranslated reading mode panel. The
  // returned selection is always forward. Returns null if no text in a block is
  // selected, if TranslatePdf is disabled, or if the page isn't a PDF.
  getAxSelection(selection: ReadingModeSelection, contentContainer: Node):
      SelectionWithIds|null {
    if (!isBlockTrackingEnabled()) {
      return null;
    }
    const blocks = this.getBlockRangeForSelection_(selection, contentContainer);
    if (!blocks) {
      return null;
    }
    return this.nodeStore_.getAxSelectionForBlockRange(
        blocks.startBlockId, blocks.endBlockId);
  }

  // Returns the forward selection in the reading mode panel that selects the
  // whole blocks that contain the text selected by the given forward AX
  // selection. Returns null if either endpoint has no AXNodeID, if no text in a
  // block is selected, if the blocks aren't shown in the content container, if
  // TranslatePdf is disabled, or if the page isn't a PDF.
  getReadingModeSelection(
      axSelection: SelectionWithIds,
      contentContainer: Node): ReadingModeSelection|null {
    if (!isBlockTrackingEnabled()) {
      return null;
    }
    const {anchorNodeId, anchorOffset, focusNodeId, focusOffset} = axSelection;
    if (anchorNodeId === undefined || focusNodeId === undefined) {
      return null;
    }
    const blocks = this.nodeStore_.getBlockRangeForAxSelection(
        anchorNodeId, anchorOffset, focusNodeId, focusOffset);
    if (!blocks) {
      return null;
    }
    const startBlock =
        this.getBlockElement_(blocks.startBlockId, contentContainer);
    const endBlock = this.getBlockElement_(blocks.endBlockId, contentContainer);
    if (!startBlock || !endBlock) {
      return null;
    }

    // The NodeStore keeps blocks in tree order, so the start block never comes
    // after the end block, and the selection is forward.
    const start = getPointBefore(startBlock);
    const end = getPointAfter(endBlock);
    return {
      anchorNode: start.node,
      anchorOffset: start.offset,
      focusNode: end.node,
      focusOffset: end.offset,
    };
  }

  // Returns the AXNodeID of the innermost block that contains the given node,
  // or null if the node isn't in a block in the content container.
  private getEnclosingBlockId_(node: Node, contentContainer: Node): number
      |null {
    if (!contentContainer.contains(node)) {
      return null;
    }
    for (let current: Node|null = node; current && current !== contentContainer;
         current = current.parentNode) {
      if (current.nodeType !== Node.ELEMENT_NODE) {
        continue;
      }
      const id = this.nodeStore_.getAxId(current);
      if (id !== undefined && this.nodeStore_.isBlock(id)) {
        return id;
      }
    }
    return null;
  }

  // Returns the first and last blocks that contain selected text, or null if
  // no text in a block is selected.
  private getBlockRangeForSelection_(
      selection: ReadingModeSelection, contentContainer: Node): BlockRange
      |null {
    // A collapsed selection doesn't select any text.
    if (selection.anchorNode === selection.focusNode &&
        selection.anchorOffset === selection.focusOffset) {
      return null;
    }

    const anchor = {node: selection.anchorNode, offset: selection.anchorOffset};
    const focus = {node: selection.focusNode, offset: selection.focusOffset};
    const isBackward = isBackwardSelection(
        anchor.node, anchor.offset, focus.node, focus.offset);
    const start = isBackward ? focus : anchor;
    const end = isBackward ? anchor : focus;

    // The searches skip whitespace and text outside of blocks, so an endpoint
    // in whitespace, even in another block, doesn't make a search fail; that
    // block just isn't included unless other text in it is selected. If any
    // text in a block is selected, it's at or after the start and before the
    // end, so both searches find text. So if either search finds nothing, no
    // text in a block is selected.
    const startText =
        this.getFirstSelectableTextAtOrAfter_(start, end, contentContainer);
    const endText =
        this.getLastSelectableTextBefore_(start, end, contentContainer);
    if (!startText || !endText) {
      return null;
    }

    // If no text in a block is selected, e.g. if only whitespace is selected,
    // the searches go past each other: the first search finds text after the
    // selection, and the last search finds text before it.
    if (startText.textNode !== endText.textNode &&
        !(startText.textNode.compareDocumentPosition(endText.textNode) &
          Node.DOCUMENT_POSITION_FOLLOWING)) {
      return null;
    }

    return {startBlockId: startText.blockId, endBlockId: endText.blockId};
  }

  // Returns the first text node at or after the start of the selection from
  // start to end that has selectable text, along with the AXNodeID of its
  // block. See getSelectableTextBlockId_. The text node can be after the end
  // if no text in a block is selected.
  private getFirstSelectableTextAtOrAfter_(
      start: BoundaryPoint, end: BoundaryPoint,
      contentContainer: Node): SelectableText|null {
    // Save the block ID found for the returned text node, so that it doesn't
    // need to be found again.
    let blockId: number|null = null;
    const textNode = getFirstTextNodeAtOrAfter(
        start.node, start.offset, contentContainer, candidate => {
          blockId = this.getSelectableTextBlockId_(
              candidate, start, end, contentContainer);
          return blockId !== null;
        });
    return (textNode && blockId !== null) ? {textNode, blockId} : null;
  }

  // Returns the last text node before the end of the selection from start to
  // end that has selectable text, along with the AXNodeID of its block. See
  // getSelectableTextBlockId_. The text node can be before the start if no
  // text in a block is selected.
  private getLastSelectableTextBefore_(
      start: BoundaryPoint, end: BoundaryPoint,
      contentContainer: Node): SelectableText|null {
    // Save the block ID found for the returned text node, so that it doesn't
    // need to be found again.
    let blockId: number|null = null;
    const textNode = getLastTextNodeBefore(
        end.node, end.offset, contentContainer, candidate => {
          blockId = this.getSelectableTextBlockId_(
              candidate, start, end, contentContainer);
          return blockId !== null;
        });
    return (textNode && blockId !== null) ? {textNode, blockId} : null;
  }

  // Returns the AXNodeID of the innermost block that contains the text node,
  // or null if the text node isn't in a block or the part of its text that's
  // selected by the selection from start to end is only whitespace. See
  // getSelectedText. The whitespace is checked first, since it's cheaper than
  // finding the block.
  private getSelectableTextBlockId_(
      textNode: Text, start: BoundaryPoint, end: BoundaryPoint,
      contentContainer: Node): number|null {
    if (!hasNonEmptyText(getSelectedText(textNode, start, end))) {
      return null;
    }
    return this.getEnclosingBlockId_(textNode, contentContainer);
  }

  // Returns the element for the given block if it's shown in the content
  // container.
  private getBlockElement_(blockId: number, contentContainer: Node): Element
      |null {
    const node = this.nodeStore_.getDomNode(blockId);
    if (!(node instanceof Element) || !node.isConnected ||
        node === contentContainer || !contentContainer.contains(node)) {
      return null;
    }
    return node;
  }
}

// Returns whether the text has any characters other than whitespace, i.e.
// whether it's non-empty after trimming whitespace.
function hasNonEmptyText(text: string): boolean {
  return text.trim().length > 0;
}

// Returns the selected part of the text node's text. It starts at the start
// point if that's in the text node, or else at the start of the text, and ends
// at the end point if that's in the text node, or else at the end of the text.
function getSelectedText(
    textNode: Text, start: BoundaryPoint, end: BoundaryPoint): string {
  const startOffset = (start.node === textNode) ? start.offset : 0;
  const endOffset = (end.node === textNode) ? end.offset : textNode.length;
  return textNode.data.substring(startOffset, endOffset);
}

// Returns the boundary point just before the given node in its parent. The
// offset is the node's index among its parent's children, computed by counting
// its previous siblings to avoid copying the parent's child list.
function getPointBefore(node: Node): BoundaryPoint {
  const parent = node.parentNode!;
  let offset = 0;
  for (let current = node.previousSibling; current;
       current = current.previousSibling) {
    offset++;
  }
  return {node: parent, offset};
}

// Returns the boundary point just after the given node in its parent.
function getPointAfter(node: Node): BoundaryPoint {
  const point = getPointBefore(node);
  return {node: point.node, offset: point.offset + 1};
}
