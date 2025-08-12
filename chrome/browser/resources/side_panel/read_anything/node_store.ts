// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import {getWordCount, isRectMostlyVisible} from './common.js';

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

// The delay after which to estimate the number of words being read by the user.
// TODO(crbug.com/c/372890165): Consider making this delay a function of the
// the current size of the window and font size. Larger windows and smaller
// fonts will contain more words, where smaller windows and larger fonts will
// contain less words.
export const COUNT_WORDS_SEEN_DELAY_MS = 10 * 1000;

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

  // Accumulation of the text nodes seen by the user. Should be a subset of the
  // current nodes in domNodeToAxNodeIdMap_. Used to estimate the number of
  // words read by the user via reading mode.
  private textNodesSeen_: Set<Text> = new Set();
  private countWordsTimer?: number;

  clear() {
    this.hiddenImageNodesIds_.clear();
    this.imageNodeIdsToFetch_.clear();
    this.clearDomNodes();
  }

  clearDomNodes() {
    this.domNodeToAxNodeIdMap_.clear();
    this.textNodesSeen_.clear();
    clearTimeout(this.countWordsTimer);
    this.countWordsTimer = undefined;
  }

  clearHiddenImageNodes() {
    this.hiddenImageNodesIds_.clear();
  }

  // After a delay, uses a heuristic to estimate the number of words being read
  // by the user in reading mode. The delay is to ignore blocks of text that are
  // only viewed briefly, likely meaning the user didn't actually read it.
  estimateWordsSeenWithDelay() {
    clearTimeout(this.countWordsTimer);
    this.countWordsTimer = setTimeout(() => {
      this.estimateWordsSeen_();
    }, COUNT_WORDS_SEEN_DELAY_MS);
  }

  // Estimates the number of words that have been read by the user.
  // This heuristic assumes:
  //   - Text nodes that are fully visible are being read.
  //   - If only a small portion of a text node is offscreen, that text is
  //     likely being read.
  //   - If only a small portion of a text node is on-screen, that text is
  //     is likely not being read. If the user wants to read it, they will
  //     likely scroll so more of it is in view and it will be counted then.
  // If updating the heuristic for estimating words seen, please update the
  // assumptions listed above.
  private estimateWordsSeen_() {
    const textNodes: Text[] = Array.from(this.domNodeToAxNodeIdMap_.keys())
                                  .filter(node => node instanceof Text);

    // Add the text nodes that are currently visible. textNodesSeen_ is a Set so
    // that if the user scrolls and a previously seen text node is still in
    // view, it's not counted twice. Nodes that were previously marked as "seen"
    // will remain in the set, and the total count will be re-calculated from
    // the total set of seen nodes, including both the old and new ones.
    for (const textNode of textNodes) {
      const bounds = textNode.parentElement?.getBoundingClientRect();
      // Only add text nodes that are significantly within the visible window.
      // If a text node is only slightly visible, then it's less likely the user
      // is actually reading that text.
      if (bounds && isRectMostlyVisible(bounds)) {
        this.textNodesSeen_.add(textNode);
      }
    }
    const wordsSeen =
        Array.from(this.textNodesSeen_).reduce((totalCount, currentNode) => {
          const text = currentNode.textContent?.trim();
          if (!text || !text.length) {
            return totalCount;
          }
          return totalCount + getWordCount(text);
        }, 0);
    chrome.readingMode.updateWordsSeen(wordsSeen);
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
