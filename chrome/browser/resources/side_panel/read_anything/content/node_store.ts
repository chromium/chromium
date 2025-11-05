// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {AncestorNode, ReadAloudNode} from '../read_aloud/read_aloud_types.js';
import {getWordCount, isRectMostlyVisible} from '../shared/common.js';

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

// Use a high estimate of words per minute (average is 200-300) to account for
// skimming, and because we want to err on the side of logging more words read
// rather than on the side of less words read.
export const ESTIMATED_WORDS_PER_MS = 500 / (60 * 1000);
// If the number of words showing is small enough, then the calculated delay
// after which we count words seen will be microscopic. Use this minimum delay
// to ensure we don't log words as seen if they're only onscreen for a very
// short amount of time (e.g. if the user is selecting text to distill, then
// content may only contain a couple words and may be updated in rapid
// succession).
export const MIN_MS_TO_READ = 3 * 1000;

// Stores the nodes used in reading mode for access across the reading mode
// app.
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
  private countWordsTimer_?: number;
  private wordsSeenLastSavedTime_: number = Date.now();

  // Key: a DOM node that's already been read aloud
  // Value: the index offset at which this node's text begins within its parent
  // text. For reading aloud we sometimes split up nodes so the speech sounds
  // more natural. When that text is then selected we need to pass the correct
  // index down the pipeline, so we store that info here.
  private textNodeToAncestor_: Map<Node, AncestorNode> = new Map();

  clear() {
    this.hiddenImageNodesIds_.clear();
    this.imageNodeIdsToFetch_.clear();
    this.clearDomNodes();
  }

  clearDomNodes() {
    this.domNodeToAxNodeIdMap_.clear();
    this.textNodesSeen_.clear();
    this.wordsSeenLastSavedTime_ = Date.now();
    clearTimeout(this.countWordsTimer_);
    this.countWordsTimer_ = undefined;
  }

  clearHiddenImageNodes() {
    this.hiddenImageNodesIds_.clear();
  }

  // Uses a heuristic to estimate the number of words being read by the user in
  // reading mode. The heuristic does the following:
  // 1. Estimates the number of new words visible on the screen.
  // 2. Estimates the time required for a person to read that number of words.
  // 3. Starts a timer for that duration. If the timer completes without
  //    interruption, the words are counted as "seen."
  // 4. Dynamically extends the timer if new activity occurs (like scrolling).
  //    It adjusts the total wait time to account for the new total number of
  //    words, ensuring the final count is saved only after the total estimated
  //    reading time has passed.
  estimateWordsSeenWithDelay() {
    clearTimeout(this.countWordsTimer_);
    const textShownSinceLastSave = this.estimateTextShownSinceLastSave_();
    const newWordsShown = this.estimateWordCount_(textShownSinceLastSave);
    // Estimate the amount of time it might take a user to read the new number
    // of words seen, erring on the lower side to ensure we capture it. There is
    // no harm if the user takes longer to read, as the next count of words seen
    // would be counted again after they scroll to see more content or switch
    // pages.
    const timeToReadMs = Math.floor(newWordsShown / ESTIMATED_WORDS_PER_MS);
    // Some time may have already elapsed since the words read were last saved.
    // If the count of words continues to increase before saving them (e.g. the
    // user scrolls a little bit as they read a paragraph), then account for the
    // time already spent reading the text, and save the words read after the
    // total amount of time elapsed since last saved is the estimate for the
    // time needed for the total number of words read since last saved.
    const msSinceLastSave = Date.now() - this.wordsSeenLastSavedTime_;
    const delayMs = Math.max(timeToReadMs - msSinceLastSave, MIN_MS_TO_READ);
    // Only mark the new text as "seen" after the user has had approximately
    // enough time to read it.
    this.countWordsTimer_ = setTimeout(() => {
      this.saveWordsSeen_(textShownSinceLastSave);
    }, delayMs);
  }

  // Estimates the text content that could be read by the user.
  // This heuristic assumes:
  //   - Text nodes that are fully visible are being read.
  //   - If only a small portion of a text node is offscreen, that text is
  //     likely being read.
  //   - If only a small portion of a text node is on-screen, that text is
  //     is likely not being read. If the user wants to read it, they will
  //     likely scroll so more of it is in view and it will be counted then.
  // If updating the heuristic for estimating words seen, please update the
  // assumptions listed above.
  //
  // Returns the new estimated text content that has not yet been saved as read.
  private estimateTextShownSinceLastSave_(): Set<Text> {
    const textNodes: Text[] = Array.from(this.domNodeToAxNodeIdMap_.keys())
                                  .filter(node => node instanceof Text);
    const newTextSeen: Set<Text> = new Set();
    // Add the text nodes that are currently visible. textNodesSeen_ is a Set so
    // that if the user scrolls and a previously seen text node is still in
    // view, it's not counted twice. Nodes that were previously marked as "seen"
    // will remain in the set, and the total count will be re-calculated from
    // the total set of seen nodes, including both the old and new ones.
    for (const textNode of textNodes) {
      if (this.textNodesSeen_.has(textNode)) {
        continue;
      }
      const bounds = textNode.parentElement?.getBoundingClientRect();
      // Only add text nodes that are significantly within the visible window.
      // If a text node is only slightly visible, then it's less likely the user
      // is actually reading that text.
      if (bounds && isRectMostlyVisible(bounds)) {
        newTextSeen.add(textNode);
      }
    }
    return newTextSeen;
  }

  // Estimates the number of words contained in the given text nodes.
  private estimateWordCount_(texts: Set<Text>): number {
    return Array.from(texts).reduce((totalCount, currentNode) => {
      const text = currentNode.textContent?.trim();
      return text ? totalCount + getWordCount(text) : totalCount;
    }, 0);
  }

  // Calculates and stores the accumulative total words seen by the user on this
  // page. This transitions the given set of Text nodes from being considered
  // "shown" to being considered "seen".
  private saveWordsSeen_(textShownSinceLastSave: Set<Text>): void {
    this.wordsSeenLastSavedTime_ = Date.now();
    textShownSinceLastSave.forEach(node => this.textNodesSeen_.add(node));
    const wordsSeen = this.estimateWordCount_(this.textNodesSeen_);
    chrome.readingMode.updateWordsSeen(wordsSeen);
  }

  hasAnyNode(nodes: ReadAloudNode[]): boolean {
    return nodes.some(node => node && node.domNode() !== undefined);
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
    // When Screen2x is used with the V8 segmentation model, a node id should
    // never be undefined. However, when Readability.js is used, nodes will not
    // be added to the node_store in updateContent, as there aren't any
    // associated AxNodeIds.
    // With the TS text segmentation implementation without Readability, the
    // majority of nodes will have associated AXNodeIDs, but it's possible for
    // some nodes (such as ordered lists) to not have an associated AXNodeId.
    // Longer term, this should be consolidated, but in the short term,
    // continue asserting when these flags are disabled avoid introducing new
    // bugs, while ignoring the requirement for Readability.js in order for
    // highlighting to work with Readability and with the TS text segmentation
    // model.
    if (!chrome.readingMode.isReadabilityEnabled &&
        !chrome.readingMode.isTsTextSegmentationEnabled) {
      assert(
          nodeId !== undefined,
          'trying to replace an element that doesn\'t exist');
    }

    if (nodeId !== undefined) {
      // Update map.
      this.removeDomNode(current);
      this.setDomNode(replacer, nodeId);
    }
    // Replace element in DOM.
    current.replaceWith(replacer);
  }

  hideImageNode(nodeId: number): void {
    this.hiddenImageNodesIds_.add(nodeId);
  }

  // TODO: crbug.com/440400392- Handle hidden image node ids for read aloud
  // when non-AXNode-based read aloud nodes are used.
  areNodesAllHidden(nodes: ReadAloudNode[]): boolean {
    return nodes.every(node => {
      const domNode = node && node.domNode();
      const id = domNode && this.getAxId(domNode);
      return !!id && this.hiddenImageNodesIds_.has(id);
    });
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

  setAncestor(node: ChildNode, ancestor: ParentNode, offsetInAncestor: number) {
    this.textNodeToAncestor_.set(node, {
      node: ancestor,
      offset: offsetInAncestor,
    });
  }

  getAncestor(node: Node): AncestorNode|undefined {
    if (this.textNodeToAncestor_.has(node)) {
      return this.textNodeToAncestor_.get(node);
    }

    return undefined;
  }

  static getInstance(): NodeStore {
    return instance || (instance = new NodeStore());
  }

  static setInstance(obj: NodeStore) {
    instance = obj;
  }
}

let instance: NodeStore|null = null;
