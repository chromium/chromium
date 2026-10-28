// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Segment} from '../read_aloud/read_aloud_types.js';

// Given a root node and a start index, returns the text node and its offset
// within the root node that contains the start index.
export function getTextNodeOffsets(
    rootNode: Node, start: number): {node: Node, offset: number} {
  let offset = 0;
  if (rootNode.nodeType === Node.TEXT_NODE) {
    return {node: rootNode, offset};
  }

  const treeWalker = document.createTreeWalker(rootNode, NodeFilter.SHOW_TEXT);
  let lastTextNode: Node|null = null;
  let lastOffset = 0;

  while (treeWalker.nextNode()) {
    const textNode = treeWalker.currentNode;
    const length = textNode.textContent!.length;
    lastTextNode = textNode;
    lastOffset = offset;

    // Check if the target start index falls within this node's range
    // Range is [offset, offset + length)
    if (start < offset + length) {
      return {node: textNode, offset};
    }

    offset += length;
  }

  // If the target start index is exactly at the end of the last text node,
  // return the last text node to keep the selection inside text.
  if (start === offset && lastTextNode) {
    return {node: lastTextNode, offset: lastOffset};
  }

  return {node: rootNode, offset};
}

// Returns the bounding rects for the given text segments.
export function getRectsForSegments(segments: Segment[]): DOMRect[] {
  const rects: DOMRect[] = [];
  for (const {node, start} of segments) {
    const domNode = node.domNode();
    if (!domNode) {
      continue;
    }
    const {node: finalNode, offset} = getTextNodeOffsets(domNode, start);
    const startOffset = start - offset;
    const range = document.createRange();

    range.setStart(finalNode, startOffset);
    range.setEndAfter(finalNode);
    rects.push(...Array.from(range.getClientRects()));
  }

  return Array.from(new Set(rects)).sort((a, b) => a.bottom - b.bottom);
}

// Returns whether the selection's focus comes before its anchor in document
// order, i.e. the user dragged backward.
export function isBackwardSelection(
    anchorNode: Node, anchorOffset: number, focusNode: Node,
    focusOffset: number): boolean {
  const position = anchorNode.compareDocumentPosition(focusNode);
  if (position === Node.DOCUMENT_POSITION_PRECEDING) {
    return true;
  }
  // If the focus and anchor are the same node, compare the offsets.
  return position === 0 && focusOffset < anchorOffset;
}

// Normalizes a raw DOM selection boundary point (node, offset) into a clean,
// visual leaf Text node and character offset representation.
//
// Normalization is needed for two primary reasons:
// 1. Mapping Block Elements: Browsers often set selection boundaries to parent
//    block containers (like <p> or <div> elements, with offsets as child
//    indexes) on triple-clicks.
// 2. Boundary Shifting: Selection endpoints sitting on text node edges (offset
//    0 for end-points or offset textContent.length for start-points) are
//    shifted backward or forward into the closest valid text nodes (skipping
//    unmapped layout spacers or whitespace-only nodes). This maintains
//    contiguity and prevents highlight "leaking" across non-distilled visual
//    boundaries (like Wikipedia infoboxes, compositions, or sidebars) in the
//    main page frame.
export function getNearestTextBoundaryPoint(
    node: Node, offset: number, container: Node, isStart: boolean,
    isValidTarget?: (node: Node) => boolean): {node: Node, offset: number} {
  // If the node is already a leaf Text node and the offset lies strictly
  // inside the text, no normalization is needed.
  if (node.nodeType === Node.TEXT_NODE) {
    if (offset > 0 && offset < (node.textContent?.length ?? 0)) {
      return {node, offset};
    }
  }

  let targetNode = node;
  let targetOffset = offset;

  // If the endpoint is an Element (block container), traverse to find its
  // nearest matching leaf Text node.
  if (node instanceof Element) {
    // - If offset < length, the boundary point is located BEFORE
    // node.childNodes[offset].
    // - If offset >= length, the boundary point has wrapped AFTER the last
    // child node of the element.
    if (offset < node.childNodes.length) {
      const result = getTextNodeByForwardSearch(node, offset, container);
      targetNode = result.node;
      targetOffset = result.offset;
    } else {
      const result = getTextNodeByBackwardSearch(node, container);
      targetNode = result.node;
      targetOffset = result.offset;
    }
  }

  // Shift End Boundary Backward:
  // If the end of the selection is at the very start of a text node (offset
  // 0), shift it backward to the end of the nearest preceding valid text node.
  // This maintains contiguous selection highlighting in the main pane and
  // avoids visual "leaks" past block elements (like Wikipedia infoboxes or
  // composition dividers).
  if (!isStart && targetNode.nodeType === Node.TEXT_NODE &&
      targetOffset === 0) {
    const prevText = getPreviousTextNode(targetNode, container, isValidTarget);
    if (prevText) {
      targetNode = prevText;
      targetOffset = prevText.length;
    }
  }

  // Shift Start Boundary Forward:
  // If the start of the selection is at the very end of a text node (offset
  // equals length), shift it forward to the start of the nearest succeeding
  // valid text node. This also ensures selection contiguity across block
  // element boundaries in the main pane.
  if (isStart && targetNode.nodeType === Node.TEXT_NODE &&
      targetOffset === (targetNode.textContent?.length ?? 0)) {
    const nextText = getNextTextNode(targetNode, container, isValidTarget);
    if (nextText) {
      targetNode = nextText;
      targetOffset = 0;
    }
  }

  return {node: targetNode, offset: targetOffset};
}

// Returns the first text node in the container at or after the boundary point
// (node, offset), in document order, skipping text nodes that don't satisfy
// isValidTarget, or null if there is none. If the point is inside a text node
// (offset < length), that text node is the first one considered.
export function getFirstTextNodeAtOrAfter(
    node: Node, offset: number, container: Node,
    isValidTarget?: (text: Text) => boolean): Text|null {
  // isValidTarget isn't passed to the searches below, so that it's called only
  // once on the text node found, by the check after the branches.
  let first: Text|null;
  if (node instanceof Text) {
    first = offset < node.length ? node : getNextTextNode(node, container);
  } else if (offset < node.childNodes.length) {
    // The point is before the child at the offset, so the result is the first
    // text node in the child's subtree, or the first one after it.
    const child = node.childNodes[offset]!;
    first = child instanceof Text ? child : getNextTextNode(child, container);
  } else {
    // The point is after all of the node's children, so the result is the
    // first text node after the node's subtree.
    first = getNextTextNode(getLastDescendant(node), container);
  }

  if (!first || !isValidTarget || isValidTarget(first)) {
    return first;
  }
  return getNextTextNode(first, container, isValidTarget);
}

// Returns the last text node in the container before the boundary point
// (node, offset), in document order, skipping text nodes that don't satisfy
// isValidTarget, or null if there is none. If the point is inside a text node
// (offset > 0), that text node is the first one considered.
export function getLastTextNodeBefore(
    node: Node, offset: number, container: Node,
    isValidTarget?: (text: Text) => boolean): Text|null {
  // isValidTarget isn't passed to the searches below, so that it's called only
  // once on the text node found, by the check after the branches.
  let last: Text|null;
  const index = Math.min(offset, node.childNodes.length);
  if (node instanceof Text) {
    last = offset > 0 ? node : getPreviousTextNode(node, container);
  } else if (index > 0) {
    // The point is after the child before the offset, so the result is the
    // last text node in the child's subtree, or the last one before it.
    const descendant = getLastDescendant(node.childNodes[index - 1]!);
    last = descendant instanceof Text ?
        descendant :
        getPreviousTextNode(descendant, container);
  } else {
    // The point is before all of the node's children, so the result is the
    // last text node before the node.
    last = getPreviousTextNode(node, container);
  }

  if (!last || !isValidTarget || isValidTarget(last)) {
    return last;
  }
  return getPreviousTextNode(last, container, isValidTarget);
}

// Returns the last node in document order within the given node's subtree,
// which is the node itself if it has no children.
function getLastDescendant(node: Node): Node {
  let last = node;
  while (last.lastChild) {
    last = last.lastChild;
  }
  return last;
}

// Returns the next text node in the container after the given node, in
// document order, skipping text nodes that don't satisfy isValidTarget. Returns
// null if there is none.
export function getNextTextNode(
    node: Node, container: Node, isValidTarget?: (text: Text) => boolean): Text|
    null {
  const walker = document.createTreeWalker(container, NodeFilter.SHOW_TEXT);
  walker.currentNode = node;
  let next = textNodeOrNull(walker.nextNode());
  while (next && isValidTarget && !isValidTarget(next)) {
    next = textNodeOrNull(walker.nextNode());
  }
  return next;
}

// Returns the previous text node in the container before the given node, in
// document order, skipping text nodes that don't satisfy isValidTarget. Returns
// null if there is none.
export function getPreviousTextNode(
    node: Node, container: Node, isValidTarget?: (text: Text) => boolean): Text|
    null {
  const walker = document.createTreeWalker(container, NodeFilter.SHOW_TEXT);
  walker.currentNode = node;
  let prev = textNodeOrNull(walker.previousNode());
  while (prev && isValidTarget && !isValidTarget(prev)) {
    prev = textNodeOrNull(walker.previousNode());
  }
  return prev;
}

// Narrows a TreeWalker result to Text. The walkers above only show text nodes,
// but TreeWalker's methods are typed as returning Node.
function textNodeOrNull(node: Node|null): Text|null {
  return node instanceof Text ? node : null;
}

// Locates the target text node and offset by inspecting the child at the
// given offset or traversing its subtree.
function getTextNodeByForwardSearch(
    element: Element, offset: number,
    container: Node): {node: Node, offset: number} {
  const text = getFirstTextNodeAtOrAfter(element, offset, container);
  if (text) {
    return {node: text, offset: 0};
  }

  // Fallback: Clamp to the last text node in the container.
  const lastText = getLastTextNode(container);
  if (lastText) {
    return {node: lastText, offset: lastText.textContent?.length ?? 0};
  }

  return {node: element, offset};
}

// Locates the target text node and offset by traversing the element's subtree.
function getTextNodeByBackwardSearch(
    element: Element, container: Node): {node: Node, offset: number} {
  // Locate the last text node in the element's subtree.
  const walker = document.createTreeWalker(element, NodeFilter.SHOW_TEXT);
  let lastText: Node|null = null;
  while (walker.nextNode()) {
    lastText = walker.currentNode;
  }
  if (lastText) {
    return {node: lastText, offset: lastText.textContent?.length ?? 0};
  }

  // Fallback: Walk forward or backward to find adjacent text nodes.
  const documentWalker =
      document.createTreeWalker(container, NodeFilter.SHOW_TEXT);
  documentWalker.currentNode = element;
  const nextText = documentWalker.nextNode();
  if (nextText) {
    return {node: nextText, offset: 0};
  }

  const prevText = getLastTextNode(container);
  if (prevText) {
    return {node: prevText, offset: prevText.textContent?.length ?? 0};
  }

  return {node: element, offset: element.childNodes.length};
}

// Scans the specified container DOM subtree to find and return the very last
// leaf Text node. This is used as a robust fallback endpoint during selection
// normalization when a boundary is positioned at the absolute end of a
// container.
function getLastTextNode(container: Node): Text|null {
  const walker = document.createTreeWalker(container, NodeFilter.SHOW_TEXT);
  let lastText: Text|null = null;
  while (walker.nextNode()) {
    lastText = textNodeOrNull(walker.currentNode);
  }
  return lastText;
}
