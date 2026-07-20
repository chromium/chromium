// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {createVisibleTreeWalker, getNextValidNode} from '../shared/common.js';

// Gets the next valid node for read aloud, given a root node and a starting
// node.
export function getNextValidNodeFromPosition(
    root: Node, startNode: Node|null = null): Node|null {
  const walker = createVisibleTreeWalker(root);
  if (startNode) {
    walker.currentNode = startNode;
  }
  return getNextValidNode(walker);
}
