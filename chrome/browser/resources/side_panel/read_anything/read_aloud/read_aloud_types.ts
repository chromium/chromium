// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NodeStore} from '../node_store.js';

// This file contains type definitions for the data structures
// used in the read aloud feature. It provides an abstraction layer for
// representing text nodes and segments, allowing the core logic to work
// with different text segmentation strategies.

// Wrapper class to represent a node used by read aloud. The type of node
// could be either a DOM node or an AXNode depending on what type of text
// segmentation method is used.
export abstract class ReadAloudNode {
  abstract equals(other: ReadAloudNode|undefined|null): boolean;

  abstract domNode(): Node|undefined;

  static create(node: Node, nodeStore = NodeStore.getInstance()): ReadAloudNode
      |undefined {
    if (chrome.readingMode.isTsTextSegmentationEnabled) {
      return new DomReadAloudNode(node);
    }

    const axNodeId = nodeStore.getAxId(node);
    if (axNodeId) {
      return new AxReadAloudNode(axNodeId, nodeStore);
    }

    return undefined;
  }
}

export class AxReadAloudNode extends ReadAloudNode {
  constructor(
      public readonly axNodeId: number,
      private readonly nodeStore_ = NodeStore.getInstance()) {
    super();
  }

  equals(other: ReadAloudNode|undefined|null): boolean {
    if (!(other instanceof AxReadAloudNode)) {
      return false;
    }

    return this.axNodeId === other.axNodeId;
  }

  domNode(): Node|undefined {
    return this.nodeStore_.getDomNode(this.axNodeId);
  }
}

// Represents a node used by read aloud that's based entirely on the DOM.
export class DomReadAloudNode extends ReadAloudNode {
  constructor(public readonly node: Node) {
    super();
  }

  equals(other: ReadAloudNode|undefined|null): boolean {
    if (!(other instanceof DomReadAloudNode)) {
      return false;
    }
    return this.node.isEqualNode(other.node);
  }

  getText(): string {
    return this.node.textContent || '';
  }

  domNode(): Node|undefined {
    return this.node;
  }
}


// A segment of text within a single node.
export interface Segment {
  node: ReadAloudNode;
  start: number;
  length: number;
}
