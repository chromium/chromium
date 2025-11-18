// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NodeStore} from '../content/node_store.js';

import {ReadAloudNodeStore} from './read_aloud_node_store.js';

// This file contains type definitions for the data structures
// used in the read aloud feature. It provides an abstraction layer for
// representing text nodes and segments, allowing the core logic to work
// with different text segmentation strategies.

// Display types that should signal a line break.
const LINE_BREAKING_DISPLAY_TYPES =
    ['block', 'list-item', 'flex', 'grid', 'table'];

// Wrapper class to represent a node used by read aloud. The type of node
// could be either a DOM node or an AXNode depending on what type of text
// segmentation method is used.
export abstract class ReadAloudNode {
  abstract equals(other: ReadAloudNode|undefined|null): boolean;

  abstract domNode(): Node|undefined;

  // TODO: crbug.com/440400392: This method is a convenience method for working
  // with AxReadAloudNodes during the refactor and should be deleted once
  // the TSTextSegmentation flag is fully enabled.
  static createFromAxNode(
      axNodeId: number, nodeStore = NodeStore.getInstance()): ReadAloudNode
      |undefined {
    const domNode: Node|undefined = nodeStore.getDomNode(axNodeId);
    if (!domNode && !chrome.readingMode.isTsTextSegmentationEnabled) {
      // If there's no DOM node yet, it might not have gotten added to the
      // node store yet, so create an AxReadAloudNode instead.
      // TODO: crbug.com/440400392- This shouldn't be necessary but is a
      // fallback to help ensure that the text segmentation refactor does not
      // impact the V8 segmentation implementation.
      return new AxReadAloudNodeImpl(axNodeId, nodeStore);
    }
    if (!domNode) {
      return undefined;
    }
    return this.create(domNode, nodeStore);
  }

  static create(node: Node, nodeStore = NodeStore.getInstance()): ReadAloudNode
      |undefined {
    if (chrome.readingMode.isTsTextSegmentationEnabled) {
      return new DomReadAloudNodeImpl(node);
    }

    const axNodeId = nodeStore.getAxId(node);
    if (axNodeId) {
      return new AxReadAloudNodeImpl(axNodeId, nodeStore);
    }

    return undefined;
  }
}

export class AxReadAloudNode extends ReadAloudNode {
  protected constructor(
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

// TODO: crbug.com/440400392: The Impl classes are a tool for working
// with ReadAloudNodes during the refactor in order to enforce more strict
// ReadAloudNode creation. These classes should be deleted once
// the TSTextSegmentation flag is fully enabled and sooner, if possible.

// Impl class to help enforce that AxReadAloudNode should only be constructed
// by one of the #create methods in ReadAloudNode.
// This class should not be exported.
class AxReadAloudNodeImpl extends AxReadAloudNode {
  constructor(axNodeId: number, nodeStore = NodeStore.getInstance()) {
    super(axNodeId, nodeStore);
  }
}

// Represents a node used by read aloud that's based entirely on the DOM.
export class DomReadAloudNode extends ReadAloudNode {
  nearestBlockAncestor: Node|undefined = undefined;
  private isSuperscript_?: boolean;

  protected constructor(protected node: Node) {
    super();
    ReadAloudNodeStore.getInstance().register(this);
    // lineBreakingItem_ only needs to be set once.
    this.nearestBlockAncestor = this.getNearestBlockAncestor_();
  }

  equals(other: ReadAloudNode|undefined|null): boolean {
    if (!(other instanceof DomReadAloudNode)) {
      return false;
    }
    return this.node.isSameNode(other.node);
  }

  getText(): string {
    return this.node.textContent || '';
  }

  domNode(): Node|undefined {
    return this.node;
  }

  // Refresh the DOM node associated with this read aloud node if the original
  // node has been changed, such as from highlighting.
  refresh(newNode: Node) {
    this.node = newNode;
    this.nearestBlockAncestor = this.getNearestBlockAncestor_();
  }

  // Returns true if the DOM node associated with this ReadAloudNode is
  // part of a superscript (i.e., inside a <sup> tag).
  isSuperscript(): boolean {
    // Don't recalculate whether this node is a superscript if we've already
    // checked.
    if (this.isSuperscript_ !== undefined) {
      return this.isSuperscript_;
    }

    const domNode = this.node;
    if (!domNode) {
      this.isSuperscript_ = false;  // Cache the result
      return false;
    }
    const element = domNode.nodeType === Node.ELEMENT_NODE ?
        domNode as Element :
        domNode.parentElement;

    this.isSuperscript_ = !!element?.closest('sup');
    return this.isSuperscript_;
  }

  // The nearest ancestor to the DOM node associated with this ReadAloudNode
  // that is of a "block" style such that it would constitute a line break.
  getBlockAncestor(): Node|undefined {
    return this.nearestBlockAncestor;
  }

  private getNearestBlockAncestor_(): Node|undefined {
    let currentAncestor = this.node.parentElement;

    while (currentAncestor) {
      const displayStyle = window.getComputedStyle(currentAncestor).display;
      // Check for common block-level display values
      if (LINE_BREAKING_DISPLAY_TYPES.includes(displayStyle)) {
        return currentAncestor;
      }
      currentAncestor = currentAncestor.parentElement;
    }

    return undefined;
  }
}

// Impl class to help enforce that DomReadAloudNode should only be constructed
// by one of the #create methods in ReadAloudNode.
// This class should not be exported.
class DomReadAloudNodeImpl extends DomReadAloudNode {
  constructor(node: Node) {
    super(node);
  }
}

// A segment of text within a single node.
export interface Segment {
  node: ReadAloudNode;
  start: number;
  length: number;
}

// Represents a textual sentence.
export interface Sentence {
  text: string;
  index: number;
}

// Represents a segmented sentence to be used with read aloud that contains
// mappings to ReadAloudNodes.
export interface SegmentedSentence {
  sentenceInfo: Sentence;
  segments: Segment[];
}

// A representation of a node linked with its starting offset within a broader
// block of text
export interface OffsetByNode {
  node: DomReadAloudNode;
  startOffset: number;
}

// For reading aloud the DOM is adjusted to highlight along with speech. When
// that text is then selected, the offset text with the new DOM is needed.
export interface AncestorNode {
  node: Node;
  offset: number;
}
