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

  getHighlightForCurrentSegmentIndex(index: number, phrases: boolean):
      Segment[] {
    if (this.currentIndex_ < 0 || !this.sentences_[this.currentIndex_]) {
      return [];
    }
    const currentSentence = this.sentences_[this.currentIndex_]!;

    // TODO: crbug.com/440400392 - Implement phrase highlighting.
    if (phrases) {
      // For now, just return the current sentence.
      return this.getCurrentTextSegments();
    }

    // Word highlighting.
    return this.getWordHighlightSegment(currentSentence, index);
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

  resetModel() {
    this.resetState_();
  }


  onNodeWillBeDeleted(deletedNode: Node) {
    if (!this.isInitialized()) {
      return;
    }

    const oldCurrentIndex = this.currentIndex_;

    // Filter out any segments containing the now deleted node.
    this.sentences_.forEach(sentence => {
      sentence.segments = sentence.segments.filter(
          segment => !segment.node.domNode()?.isEqualNode(deletedNode));
    });

    // Before filtering the main sentences list, count how many sentences
    // that came before the current sentence are now empty.
    let numRemovedBeforeCurrent = 0;
    if (oldCurrentIndex > 0) {
      for (let i = 0; i < oldCurrentIndex; i++) {
        if (this.sentences_[i]!.segments.length === 0) {
          numRemovedBeforeCurrent++;
        }
      }
    }

    // Now, filter out all empty sentences from the main list.
    this.sentences_ =
        this.sentences_.filter(sentence => sentence.segments.length > 0);

    if (oldCurrentIndex !== -1) {
      // The new index is the old index, shifted by the number of sentences
      // that were removed before it.
      this.currentIndex_ = oldCurrentIndex - numRemovedBeforeCurrent;

      // Ensure currentIndex is not out of bounds.
      if (this.currentIndex_ >= this.sentences_.length) {
        this.currentIndex_ =
            this.sentences_.length > 0 ? this.sentences_.length - 1 : -1;
      }
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
    for (let i = 0; i < textNodes.length; i++) {
      const textNode = textNodes[i];
      if (!textNode) {
        continue;
      }
      nodeOffsets.push({node: textNode, startOffset: fullText.length});
      fullText += textNode.getText();

      // If there's a node after this one, check to see if there should be
      // a line break between this node and the next. If there is, add a
      // newline character to ensure both nodes aren't read as part of the same
      // sentence.
      if (i < textNodes.length - 1) {
        const nextNode = textNodes[i + 1];
        if (nextNode && this.isLineBreakingItem(textNode, nextNode)) {
          fullText += '\n';
        }
      }
    }
    return {fullText, nodeOffsets};
  }

  private isLineBreakingItem(node1: DomReadAloudNode, node2: DomReadAloudNode):
      boolean {
    const blockAncestor1 = node1.getBlockAncestor();
    const blockAncestor2 = node2.getBlockAncestor();

    if (blockAncestor1 && blockAncestor2 && blockAncestor1 !== blockAncestor2) {
      return true;
    }

    return false;
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

  private getWordHighlightSegment(
      currentSentence: SegmentedSentence, index: number): Segment[] {
    const sentenceText = currentSentence.sentenceInfo.text;
    const remainingText = sentenceText.substring(index);
    const wordEndInRemaining =
        this.textSegmenter_.getNextWordEnd(remainingText);
    const highlightEndIndex = index + wordEndInRemaining;

    const sentenceSegments = currentSentence.segments;
    const highlightSegments: Segment[] = [];
    let textSoFarIndex = 0;

    for (const segment of sentenceSegments) {
      const segmentStart = textSoFarIndex;

      // Stop iterating if segmentStart is ever greater than the highlight end
      // index.
      if (segmentStart >= highlightEndIndex) {
        break;
      }

      const highlightSegment = this.createHighlightSegment(
          segment,
          segmentStart,
          index,  // highlightStart
          highlightEndIndex,
      );

      if (highlightSegment) {
        highlightSegments.push(highlightSegment);
      }

      textSoFarIndex += segment.length;
    }

    return highlightSegments;
  }

  // Returns the part of the given sentenceSegment that should be highlighted.
  private createHighlightSegment(
      sentenceSegment: Segment,
      segmentStart: number,
      highlightStart: number,
      highlightEnd: number,
      ): Segment|null {
    const segmentEnd = segmentStart + sentenceSegment.length;

    // If the segment is entirely before or after the highlight
    // range, the highlight does not overlap with any valid part of the segment,
    // so there can be no valid highlight.
    if (segmentEnd <= highlightStart || segmentStart >= highlightEnd) {
      return null;
    }

    // Find the boundaries of region of the highlight that overlaps with the
    // segment.
    const overlapStart = Math.max(highlightStart, segmentStart);
    const overlapEnd = Math.min(highlightEnd, segmentEnd);
    const overlapLength = overlapEnd - overlapStart;

    if (overlapLength > 0) {
      return {
        node: sentenceSegment.node,
        // Adjust the start position relative to the beginning of the original
        // segment's node.
        start: sentenceSegment.start + (overlapStart - segmentStart),
        length: overlapLength,
      };
    }

    return null;
  }
}
