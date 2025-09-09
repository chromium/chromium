// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ReadAloudModelBrowserProxy} from './read_aloud_model_browser_proxy.js';
import type {OffsetByNode, Segment, SegmentedSentence, Sentence} from './read_aloud_types.js';
import {DomReadAloudNode, ReadAloudNode} from './read_aloud_types.js';
import {TextSegmenter} from './text_segmenter.js';


// Read aloud model implementation based in TS to be used when the
// ReadAnythingReadAloudTSTextSegmentation flag is enabled.
export class TsReadModelImpl implements ReadAloudModelBrowserProxy {
  private textSegmenter_: TextSegmenter = TextSegmenter.getInstance();
  // The list of sentences on the current page.
  private sentences_: SegmentedSentence[] = [];
  private currentIndex_: number = -1;
  private initialized_: boolean = false;

  // TODO: crbug.com/440400392- Sentence highlighting won't work consistently
  // until NodeStore is updated to refresh the DomNodes for highlighting.
  getHighlightForCurrentSegmentIndex(_index: number, _phrases: boolean):
      Segment[] {
    // TODO: crbug.com/440400392 - Implement word and phrase highlighting.
    // For now, just return the current sentence.
    return this.getCurrentTextSegments();
  }

  getCurrentTextSegments(): Segment[] {
    if (this.currentIndex_ === -1 || !this.sentences_[this.currentIndex_]) {
      return [];
    }
    return this.sentences_[this.currentIndex_]!.segments;
  }

  getCurrentTextContent(): string {
    if (this.currentIndex_ === -1 || !this.sentences_[this.currentIndex_]) {
      return '';
    }
    return this.sentences_[this.currentIndex_]!.sentenceInfo.text;
  }

  getAccessibleText(text: string, maxSpeechLength: number): string {
    return text.slice(
        0,
        TextSegmenter.getInstance().getAccessibleBoundary(
            text, maxSpeechLength));
  }

  resetSpeechToBeginning(): void {
    if (this.sentences_.length > 0) {
      this.currentIndex_ = 0;
    } else {
      this.currentIndex_ = -1;
    }
  }

  moveSpeechForward(): void {
    if (this.currentIndex_ < this.sentences_.length - 1) {
      this.currentIndex_++;
    } else {
      // Reached the end. Mark as finished.
      this.currentIndex_ = -1;
    }
  }

  moveSpeechBackwards(): void {
    if (this.currentIndex_ > 0) {
      this.currentIndex_--;
    }
  }

  isInitialized(): boolean {
    return this.initialized_;
  }

  init(context: DomReadAloudNode): void {
    if (!(context instanceof DomReadAloudNode)) {
      return;
    }

    this.resetState_();
    const textNodes = this.getAllTextNodesFrom_(context.domNode());
    if (!textNodes.length) {
      return;
    }

    // Gather the text from all of the text nodes and their offset within
    // the entire block of text.
    const {fullText, nodeOffsets} = this.buildTextAndOffsets_(textNodes);

    // Use TextSegmenter to get the list of sentences making up the text.
    const sentences = this.textSegmenter_.getSentences(fullText);

    // Now map the list of sentences to an array of an array of text segments.
    // Each list of segments represents a sentence that should be spoken.
    this.sentences_ = this.mapSentencesToSegments_(sentences, nodeOffsets);

    this.initialized_ = true;
    if (this.sentences_.length > 0) {
      this.currentIndex_ = 0;
    }
  }

  private resetState_() {
    this.sentences_ = [];
    this.currentIndex_ = -1;
    this.initialized_ = false;
  }

  // Takes a list of nodes to process and returns an object containing the
  // concatenated text content and a list of nodes with their starting offset
  // in their node in the concatenated text.
  private buildTextAndOffsets_(textNodes: DomReadAloudNode[]): {
    fullText: string,
    nodeOffsets: OffsetByNode[],
  } {
    let fullText = '';
    const nodeOffsets: OffsetByNode[] = [];
    for (const textNode of textNodes) {
      nodeOffsets.push({node: textNode, startOffset: fullText.length});
      fullText += textNode.getText();
    }
    return {fullText, nodeOffsets};
  }

  // Maps sentence boundaries from the concatenated text back to their
  // original DOM nodes.
  // Returns a list of sentences, where each sentence is a list of segments.
  private mapSentencesToSegments_(
      sentences: Sentence[], nodeOffsets: OffsetByNode[]): SegmentedSentence[] {
    const sentenceSegments: SegmentedSentence[] = [];
    let nodeIndex = 0;
    for (const sentence of sentences) {
      const sentenceStart = sentence.index;
      const sentenceEnd = sentence.index + sentence.text.length;
      const segments: Segment[] = [];

      for (let i = nodeIndex; i < nodeOffsets.length; i++) {
        const offsetByNode = nodeOffsets[i]!;
        const nodeLength = offsetByNode.node.getText().length;
        const nodeEndOffset = offsetByNode.startOffset + nodeLength;

        // If this node is completely after the current sentence, we can stop
        // searching for this sentence.
        if (offsetByNode.startOffset >= sentenceEnd) {
          break;
        }

        // If this node is completely before the current sentence, we can
        // skip it and start the next sentence's search from the next node.
        if (nodeEndOffset <= sentenceStart) {
          nodeIndex = i + 1;
          continue;
        }

        // There is an overlap.
        const overlapStart = Math.max(sentenceStart, offsetByNode.startOffset);
        const overlapEnd = Math.min(sentenceEnd, nodeEndOffset);
        const segment: Segment = {
          node: offsetByNode.node,
          start: overlapStart - offsetByNode.startOffset,
          length: overlapEnd - overlapStart,
        };
        if (segment.length > 0) {
          segments.push(segment);
        }
      }

      if (segments.length > 0) {
        sentenceSegments.push({sentenceInfo: sentence, segments: segments});
      }
    }
    return sentenceSegments;
  }

  private getAllTextNodesFrom_(node: Node|undefined): DomReadAloudNode[] {
    const textNodes: DomReadAloudNode[] = [];
    if (!node) {
      return textNodes;
    }
    const treeWalker = document.createTreeWalker(node, NodeFilter.SHOW_TEXT);
    let currentNode;
    while (currentNode = treeWalker.nextNode()) {
      if (currentNode.textContent && currentNode.textContent.trim().length) {
        const node = ReadAloudNode.create(currentNode);
        if (node instanceof DomReadAloudNode) {
          textNodes.push(node);
        }
      }
    }
    return textNodes;
  }
}
