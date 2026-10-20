// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isBlockTrackingEnabled} from '../shared/common.js';

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
