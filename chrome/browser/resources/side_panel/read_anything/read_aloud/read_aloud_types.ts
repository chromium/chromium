// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NodeStore} from '../content/node_store.js';

import {ReadAloudNodeStore} from './read_aloud_node_store.js';

// This file contains type definitions for the data structures
// used in the read aloud feature. It provides an abstraction layer for
// representing text nodes and segments, allowing the core logic to work
// with different text segmentation strategies.

// Set of display types that signal a line break. Using a Set allows O(1)
// lookup instead of array scanning during text traversal.
const LINE_BREAKING_DISPLAY_TYPES = new Set([
  'block',
  'list-item',
  'flex',
  'grid',
  'table',
  'table-row',
  'table-cell',
  'table-caption',
  'table-row-group',
  'table-header-group',
  'table-footer-group',
]);

// Wrapper class to represent a node used by read aloud.
// TODO(b/444029483): Remove the wrapper class since DomReadAloudNode is
// the only child.
export abstract class ReadAloudNode {
  abstract equals(other: ReadAloudNode|undefined|null): boolean;

  abstract domNode(): Node|undefined;

  // Creates a ReadAloudNode for the DOM node associated with the given
  // AXNodeID, if there is one.
  static createFromAxNode(
      axNodeId: number, nodeStore = NodeStore.getInstance()): ReadAloudNode
      |undefined {
    const domNode: Node|undefined = nodeStore.getDomNode(axNodeId);
    if (!domNode) {
      return undefined;
    }
    return this.create(domNode);
  }

  static create(node: Node): ReadAloudNode|undefined {
    return new DomReadAloudNodeImpl(node);
  }
}

// TODO(b/444029483): The Impl class is a tool for working
// with ReadAloudNodes during the refactor in order to enforce more strict
// ReadAloudNode creation. This class should be deleted once
// the Phrase Highlighting flag is fully enabled and sooner, if possible.

// Represents a node used by read aloud that's based entirely on the DOM.
export class DomReadAloudNode extends ReadAloudNode {
  private readonly nearestBlockAncestor_: Node|undefined;
  private isSuperscript_?: boolean;

  protected constructor(protected node: Node) {
    super();
    ReadAloudNodeStore.getInstance().register(this);
    // nearestBlockAncestor_ only needs to be set once.
    this.nearestBlockAncestor_ = this.getNearestBlockAncestor_();
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
  // node has been changed, such as from highlighting. The replacement is in
  // place, so the block ancestor is unchanged.
  refresh(newNode: Node) {
    this.node = newNode;
  }

  // Returns true if the DOM node associated with this ReadAloudNode is
  // part of a superscript (i.e., inside a <sup> tag).
  // TODO(crbug.com/467180032): Add an equivalent isSubscript method to ensure
  // subscripts in PDFs are segmented properly.
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
    return this.nearestBlockAncestor_;
  }

  private getNearestBlockAncestor_(): Node|undefined {
    const nodeStore = NodeStore.getInstance();
    const parent = this.node.parentElement;
    let currentAncestor: Element|null = parent;
    while (currentAncestor) {
      // Reuse the cached block ancestor and memoize for the immediate parent.
      const cached = nodeStore.getBlockAncestor(currentAncestor);
      if (cached) {
        if (parent) {
          nodeStore.setBlockAncestor(parent, cached);
        }
        return cached;
      }

      const displayStyle = window.getComputedStyle(currentAncestor).display;
      if (LINE_BREAKING_DISPLAY_TYPES.has(displayStyle)) {
        // Cache this block element for itself and the immediate parent.
        nodeStore.setBlockAncestor(currentAncestor, currentAncestor);
        if (parent) {
          nodeStore.setBlockAncestor(parent, currentAncestor);
        }
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
  // The start offsets of each phrase within sentenceInfo.text. Undefined if
  // the phrase boundaries haven't been requested yet, and empty if they've
  // been requested but aren't available.
  phraseBoundaries?: number[];
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
