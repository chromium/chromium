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

  while (treeWalker.nextNode()) {
    const textNode = treeWalker.currentNode;
    const length = textNode.textContent!.length;

    // Check if the target start index falls within this node's range
    // Range is [offset, offset + length)
    if (start < offset + length) {
      return {node: textNode, offset};
    }

    offset += length;
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
    if (y >= previousY && y < rectBottom) {
      return (isForward || (index <= 0)) ? index : index - 1;
    }
    previousY = rectBottom;
  }
  return rects.length - 1;
}
