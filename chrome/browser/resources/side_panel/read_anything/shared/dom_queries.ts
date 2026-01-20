// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
