// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

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
    if (this.currentIndex_ < 0 || !this.sentences_[this.currentIndex_] || index < 0) {
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
    if (!(context instanceof DomReadAloudNode) || this.initialized_) {
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
    this.sentences_ =
        this.processSentencesAndMapSegments_(sentences, nodeOffsets, fullText);

    this.initialized_ = true;
    if (this.sentences_.length > 0) {
      this.currentIndex_ = 0;
    }
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
      nodeOffsets.push({
        node: textNode,
        startOffset: fullText.length,
      });
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

  // Process sentences with semantic node information to better segment the
  // text. Then map sentence boundaries from the concatenated text back to their
  // original DOM nodes.
  // Returns a list of sentences, where each sentence is a list of segments.
  private processSentencesAndMapSegments_(
      sentences: Sentence[], nodeOffsets: OffsetByNode[],
      fullText: string): SegmentedSentence[] {
    const sentenceSegments: SegmentedSentence[] = [];
    let nodeIndex = 0;

    // Buffer for the previous sentence being built, potentially including
    // superscripts from the next sentence.
    let previousSentence: Sentence|null = null;

    for (const sentence of sentences) {
      let currentSentence: Sentence|null = sentence;

      // Merge any superscripts that should be merged with the previous
      // sentence.
      if (previousSentence) {
        currentSentence = this.maybeMergeSuperscripts_(
            currentSentence, nodeOffsets, fullText, previousSentence);
      }

      // Add the completed sentence to the segments array
      if (previousSentence) {
        // Create segments for the completed sentence in the buffer
        const {segments, nextNodeIndex} = this.createSegmentsForSentence_(
            previousSentence, nodeOffsets, nodeIndex);

        if (segments.length > 0) {
          sentenceSegments.push({
            sentenceInfo: previousSentence,
            segments: segments,
          });
        }
        nodeIndex = nextNodeIndex;
      }

      // Set the buffer to the current (potentially remaining) sentence
      previousSentence = currentSentence;
    }

    if (previousSentence) {
      const {segments} = this.createSegmentsForSentence_(
          previousSentence, nodeOffsets, nodeIndex);

      if (segments.length > 0) {
        sentenceSegments.push({
          sentenceInfo: previousSentence,
          segments: segments,
        });
      }
    }

    return sentenceSegments;
  }

  // Intl.Segmenter tends to group superscripts, particularly citations, with
  // the succeeding sentence instead of the sentence. e.g.
  // This is a fact.^[1] And this is another fact.
  // is segmented as "This is a fact." and "[1] And this is another fact."
  // Instead, during postprocessing with the semantic node information, regroup
  // the superscripts with the preceding sentence.
  // currentSentence represents the current sentence that contains superscripts
  // and previousSentence represents the sentence that any superscripts in
  // currentSentence should be merged back into.
  // Returns null if the entire currentSentence was a superscript and merged
  // into previousSentence. If not, returns the remaining part of
  // currentSentence that wasn't merged.
  private moveSuperscriptsToPrecedingSentence_(
      nodes: OffsetByNode[], fullText: string, currentSentence: Sentence|null,
      previousSentence: Sentence): Sentence|null {
    assert(nodes.length > 0, 'attempting to merge superscript with no nodes');
    const superscriptNode = nodes[0]!;
    const superscriptText = superscriptNode.node.getText().trim();

    const superscriptStartIndex: number =
        currentSentence!.text.indexOf(superscriptText);
    if (superscriptStartIndex === -1 ||
        currentSentence!.text.substring(0, superscriptStartIndex).trim() !==
            '') {
      const superscriptIndexInBothSentences: number =
          (previousSentence.text + currentSentence!.text)
              .indexOf(superscriptText);
      if (superscriptIndexInBothSentences <= 0) {
        // This isn't a sentence that starts with a superscript, or there's
        // text before the superscript that shouldn't be merged with the current
        // block of superscript text.
        return currentSentence;
      }
    }

    // The sentence starts with a superscript. Merge it with the previous
    // sentence.
    const superscriptEndIndex = superscriptStartIndex + superscriptText.length;

    // Update sentence buffer to contain the superscript.
    const endOfSuperscriptInSentenceIndex =
        currentSentence!.index + superscriptEndIndex;
    previousSentence.text = fullText.substring(
        previousSentence.index, endOfSuperscriptInSentenceIndex);

    const remainder = currentSentence!.text.substring(superscriptEndIndex);

    if (remainder.trim().length > 0) {
      return {
        text: remainder,
        index: currentSentence!.index + superscriptEndIndex,
      };
    }

    return null;
  }

  // Returns the list of DomReadAloudNodes associated with the provided.
  private findNodesForSentence_(
      sentence: Sentence, nodeOffsets: OffsetByNode[]): OffsetByNode[] {
    const sentenceStart = sentence.index;
    const sentenceEnd = sentence.index + sentence.text.length;
    const nodes: OffsetByNode[] = [];
    for (const offsetByNode of nodeOffsets) {
      const nodeLength = offsetByNode.node.getText().length;
      const nodeEndOffset = offsetByNode.startOffset + nodeLength;
      if (offsetByNode.startOffset >= sentenceEnd) {
        break;
      }
      if (nodeEndOffset <= sentenceStart) {
        continue;
      }
      nodes.push(offsetByNode);
    }
    return nodes;
  }

  resetModel() {
    this.resetState_();
  }


  // Helper method to handle superscript merging
  private maybeMergeSuperscripts_(
      currentSentence: Sentence, nodeOffsets: OffsetByNode[], fullText: string,
      previousSentence: Sentence): Sentence|null {
    let finalSentence: Sentence|null = currentSentence;

    while (finalSentence) {
      const nodes = this.findNodesForSentence_(finalSentence, nodeOffsets);
      // If there are no nodes in the current sentence or the first node
      // is not a superscript, no need to continue with postprocessing.
      if (nodes.length === 0 || !nodes[0]!.node.isSuperscript()) {
        break;
      }

      const nextSentence = this.moveSuperscriptsToPrecedingSentence_(
          nodes, fullText, finalSentence, previousSentence);

      // If the sentence returned is the same as the current working sentence,
      // the superscripts were unable to be grouped.
      if (nextSentence === finalSentence) {
        break;
      }

      // A merge occurred. The next sentence is either the remainder or null.
      finalSentence = nextSentence;
    }

    // Returns the unmerged part of the sentence or null
    return finalSentence;
  }

  // Creates a list of segments for a given sentence.
  private createSegmentsForSentence_(
      sentence: Sentence,
      nodeOffsets: OffsetByNode[],
      startNodeIndex: number,
      ): {segments: Segment[], nextNodeIndex: number} {
    const sentenceStart = sentence.index;
    const sentenceEnd = sentence.index + sentence.text.length;
    const segments: Segment[] = [];
    let nextNodeIndex = startNodeIndex;

    for (let i = startNodeIndex; i < nodeOffsets.length; i++) {
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
        nextNodeIndex = i + 1;
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

    return {segments, nextNodeIndex};
  }

  private getAllTextNodesFrom_(node: Node|undefined): DomReadAloudNode[] {
    const textNodes: DomReadAloudNode[] = [];
    if (!node) {
      return textNodes;
    }
    const treeWalker = document.createTreeWalker(node, NodeFilter.SHOW_ALL);
    let currentNode;

    while (currentNode = treeWalker.nextNode()) {
      if (currentNode.nodeType === Node.ELEMENT_NODE) {
        this.addNodeForListElement(currentNode, textNodes);
      } else if (currentNode.nodeType === Node.TEXT_NODE) {
        // Don't filter out text nodes that are just whitespace. Filtering
        // out these nodes can cause two different nodes to be improperly
        // grouped as the same word.
        // e.g.
        // <a>link</a> <a>hyperlink</a> would be incorrectly spoken as
        // "linkhyperlink" instead of "link hyperlink" if the whitespace
        // was filtered out. This would cause issues with highlighting.
        if (currentNode.textContent) {
          const readAloudNode = ReadAloudNode.create(currentNode);
          if (readAloudNode instanceof DomReadAloudNode) {
            textNodes.push(readAloudNode);
          }
        }
      }
    }
    return textNodes;
  }

  private addNodeForListElement(
      currentNode: Node, textNodes: DomReadAloudNode[]) {
    const element = currentNode as HTMLElement;

    // If there is an ordered list, add the numbers as read aloud nodes, since
    // these aren't considered "text" nodes and won't be spoken by read aloud
    // otherwise.
    if (element.tagName === 'LI' && element.parentElement &&
        element.parentElement.tagName === 'OL') {
      const number = this.getLiNumber(element as HTMLLIElement);

      if (number > -1) {
        // Create the text node (e.g., "1. "). A newline is added to the
        // beginning of the node to ensure that it is not accidentally
        // grouped with the previous text node for sentence segmentation.
        const markerNode = document.createTextNode('\n' + number + '. ');
        const readAloudNode = ReadAloudNode.create(markerNode);
        if (readAloudNode instanceof DomReadAloudNode) {
          textNodes.push(readAloudNode);
        }
      }
    }
  }

  private getLiNumber(liElement: HTMLLIElement) {
    const ol = liElement.closest('ol');
    if (!ol) {
      // Not in an ordered list.
      return -1;
    }

    // Get the list's starting number. Default is 1 unless the start attribute
    // is set by the developer.
    let counter = ol.start || 1;

    // Iterate through all <li> elements in the <ol>
    for (const item of ol.children) {
      if (item.tagName !== 'LI') {
        // Skip non-<li> elements
        continue;
      }

      // If the developer set an explicit 'value' on *this* <li>, honor that.
      // If it's 0, it means the attribute isn't set.
      if ((item as HTMLLIElement).value > 0) {
        counter = (item as HTMLLIElement).value;
      }

      if (item === liElement) {
        return counter;
      }

      // It's not the selected <li>, so increment the counter for the next loop
      counter++;
    }

    // Should not happen
    return -1;
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
