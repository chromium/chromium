// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NodeStore} from '../content/node_store.js';

import {ReadAloudNodeStore} from './read_aloud_node_store.js';

// This file contains type definitions for the data structures
// used in the read aloud feature, such as the nodes, segments, and sentences
// used for text segmentation.

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

// Represents a DOM node used by read aloud.
export class ReadAloudNode {
  private readonly nearestBlockAncestor_: Node|undefined;
  private isSuperscript_?: boolean;

  // ReadAloudNodes should be created with ReadAloudNode.create() or
  // ReadAloudNode.createFromAxNode().
  private constructor(private node_: Node) {
    ReadAloudNodeStore.getInstance().register(this);
    // nearestBlockAncestor_ only needs to be set once.
    this.nearestBlockAncestor_ = this.getNearestBlockAncestor_();
  }

  static create(node: Node): ReadAloudNode {
    return new ReadAloudNode(node);
  }

  // Creates a ReadAloudNode for the DOM node associated with the given
  // AXNodeID, if there is one.
  static createFromAxNode(
      axNodeId: number, nodeStore = NodeStore.getInstance()): ReadAloudNode
      |undefined {
    const domNode: Node|undefined = nodeStore.getDomNode(axNodeId);
    if (!domNode) {
      return undefined;
    }
    return ReadAloudNode.create(domNode);
  }

  equals(other: ReadAloudNode|undefined|null): boolean {
    if (!other) {
      return false;
    }
    return this.node_.isSameNode(other.node_);
  }

  getText(): string {
    return this.node_.textContent || '';
  }

  domNode(): Node {
    return this.node_;
  }

  // Refresh the DOM node associated with this read aloud node if the original
  // node has been changed, such as from highlighting. The replacement is in
  // place, so the block ancestor is unchanged.
  refresh(newNode: Node) {
    this.node_ = newNode;
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

    const domNode = this.node_;
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
    const parent = this.node_.parentElement;
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
  node: ReadAloudNode;
  startOffset: number;
}

// For reading aloud the DOM is adjusted to highlight along with speech. When
// that text is then selected, the offset text with the new DOM is needed.
export interface AncestorNode {
  node: Node;
  offset: number;
}
