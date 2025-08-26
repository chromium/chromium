// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains type definitions for the data structures
// used in the read aloud feature. It provides an abstraction layer for
// representing text nodes and segments, allowing the core logic to work
// with different text segmentation strategies.

// Wrapper class to represent a node used by read aloud. The type of node
// could be either a DOM node or an AXNode depending on what type of text
// segmentation method is used.
export abstract class ReadAloudNode {
  abstract equals(other: ReadAloudNode|undefined|null): boolean;
}

export class AxReadAloudNode extends ReadAloudNode {
  constructor(public readonly axNodeId: number) {
    super();
  }

  equals(other: ReadAloudNode|undefined|null): boolean {
    if (!(other instanceof AxReadAloudNode)) {
      return false;
    }

    return this.axNodeId === other.axNodeId;
  }
}

// A segment of text within a single node.
export interface Segment {
  node: ReadAloudNode;
  start: number;
  length: number;
}
