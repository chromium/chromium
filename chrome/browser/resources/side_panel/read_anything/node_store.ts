// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

// A two-way map where each key is unique and each value is unique. The keys are
// DOM nodes and the values are numbers, representing AXNodeIDs.
class TwoWayMap<K, V> extends Map<K, V> {
  #reverseMap: Map<V, K>;
  constructor() {
    super();
    this.#reverseMap = new Map();
  }
  override set(key: K, value: V) {
    super.set(key, value);
    this.#reverseMap.set(value, key);
    return this;
  }
  keyFrom(value: V) {
    return this.#reverseMap.get(value);
  }
  override clear() {
    super.clear();
    this.#reverseMap.clear();
  }
  override delete(key: K): boolean {
    const v = this.get(key);
    let wasReverseDeleted = false;
    if (v) {
      wasReverseDeleted = this.#reverseMap.delete(v);
    }

    return wasReverseDeleted && super.delete(key);
  }
}

// Stores the nodes used in reading mode for access across the reading mode app.
export class NodeStore {
  // Maps a DOM node to the AXNodeID that was used to create it. DOM nodes and
  // AXNodeIDs are unique, so this is a two way map where either DOM node or
  // AXNodeID can be used to access the other.
  private domNodeToAxNodeIdMap_: TwoWayMap<Node, number> = new TwoWayMap();
  // IDs of the text nodes that are hidden when images are hidden. This is
  // usually the figcaption elements which we want to keep distilled for quick
  // turnaround when enabling/disabling images, but we don't want read aloud to
  // read out text that's not showing, so keep track of which nodes are not
  // showing.
  private hiddenImageNodesIds_: Set<number> = new Set();
  private imageNodeIdsToFetch_: Set<number> = new Set();

  clear() {
    this.domNodeToAxNodeIdMap_.clear();
    this.hiddenImageNodesIds_.clear();
    this.imageNodeIdsToFetch_.clear();
  }

  clearDomNodes() {
    this.domNodeToAxNodeIdMap_.clear();
  }

  clearHiddenImageNodes() {
    this.hiddenImageNodesIds_.clear();
  }

  getDomNode(axNodeId: number): Node|undefined {
    return this.domNodeToAxNodeIdMap_.keyFrom(axNodeId);
  }

  setDomNode(domNode: Node, axNodeId: number): void {
    this.domNodeToAxNodeIdMap_.set(domNode, axNodeId);
  }

  removeDomNode(domNode: Node): void {
    this.domNodeToAxNodeIdMap_.delete(domNode);
  }

  getAxId(domNode: Node): number|undefined {
    return this.domNodeToAxNodeIdMap_.get(domNode);
  }

  replaceDomNode(current: ChildNode, replacer: Node) {
    const nodeId = this.getAxId(current);
    assert(
        nodeId !== undefined,
        'trying to replace an element that doesn\'t exist');
    // Update map.
    this.removeDomNode(current);
    this.setDomNode(replacer, nodeId);
    // Replace element in DOM.
    current.replaceWith(replacer);
  }

  hideImageNode(nodeId: number): void {
    this.hiddenImageNodesIds_.add(nodeId);
  }

  areNodesAllHidden(nodeIds: number[]): boolean {
    return nodeIds.every(id => this.hiddenImageNodesIds_.has(id));
  }

  addImageToFetch(nodeId: number): void {
    this.imageNodeIdsToFetch_.add(nodeId);
  }

  hasImagesToFetch(): boolean {
    return this.imageNodeIdsToFetch_.size > 0;
  }

  fetchImages(): void {
    for (const nodeId of this.imageNodeIdsToFetch_) {
      chrome.readingMode.requestImageData(nodeId);
    }

    this.imageNodeIdsToFetch_.clear();
  }

  static getInstance(): NodeStore {
    return instance || (instance = new NodeStore());
  }

  static setInstance(obj: NodeStore) {
    instance = obj;
  }
}

let instance: NodeStore|null = null;
