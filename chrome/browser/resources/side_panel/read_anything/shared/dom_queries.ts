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

// Returns the index of the first rect in the given list that matches the
// given y position.
export function getRectIndexAtY(
    y: number, rects: DOMRect[], isForward: boolean): number {
  let previousY = 0;
  for (let index = 0; index < rects.length; index++) {
    const rectBottom = rects[index]!.bottom;
    if (y >= previousY && y <= rectBottom) {
      return (isForward || (index <= 0)) ? index : index - 1;
    }
    previousY = rectBottom;
  }
  return rects.length - 1;
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
  if (node.nodeType === Node.ELEMENT_NODE) {
    const element = node as Element;
    // - If offset < length, the boundary point is located BEFORE
    // element.childNodes[offset].
    // - If offset >= length, the boundary point has wrapped AFTER the last
    // child node of the element.
    if (offset < element.childNodes.length) {
      const result = getTextNodeByForwardSearch(element, offset, container);
      targetNode = result.node;
      targetOffset = result.offset;
    } else {
      const result = getTextNodeByBackwardSearch(element, container);
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
    const walker = document.createTreeWalker(container, NodeFilter.SHOW_TEXT);
    walker.currentNode = targetNode;
    let prevText = walker.previousNode();
    while (prevText && isValidTarget && !isValidTarget(prevText)) {
      prevText = walker.previousNode();
    }

    if (prevText) {
      targetNode = prevText;
      targetOffset = prevText.textContent?.length ?? 0;
    }
  }

  // Shift Start Boundary Forward:
  // If the start of the selection is at the very end of a text node (offset
  // equals length), shift it forward to the start of the nearest succeeding
  // valid text node. This also ensures selection contiguity across block
  // element boundaries in the main pane.
  if (isStart && targetNode.nodeType === Node.TEXT_NODE &&
      targetOffset === (targetNode.textContent?.length ?? 0)) {
    const walker = document.createTreeWalker(container, NodeFilter.SHOW_TEXT);
    walker.currentNode = targetNode;
    let nextText = walker.nextNode();
    while (nextText && isValidTarget && !isValidTarget(nextText)) {
      nextText = walker.nextNode();
    }

    if (nextText) {
      targetNode = nextText;
      targetOffset = 0;
    }
  }

  return {node: targetNode, offset: targetOffset};
}

// Locates the target text node and offset by inspecting the child at the
// given offset or traversing its subtree.
function getTextNodeByForwardSearch(
    element: Element, offset: number,
    container: Node): {node: Node, offset: number} {
  const child = element.childNodes[offset]!;
  if (child.nodeType === Node.TEXT_NODE) {
    return {node: child, offset: 0};
  }

  // Locate the first text node in the child's subtree.
  const walker = document.createTreeWalker(child, NodeFilter.SHOW_TEXT);
  const firstText = walker.nextNode();
  if (firstText) {
    return {node: firstText, offset: 0};
  }

  // Fallback: Walk forward to find the next text node in the document.
  const documentWalker =
      document.createTreeWalker(container, NodeFilter.SHOW_TEXT);
  documentWalker.currentNode = child;
  const nextText = documentWalker.nextNode();
  if (nextText) {
    return {node: nextText, offset: 0};
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
    lastText = walker.currentNode as Text;
  }
  return lastText;
}
