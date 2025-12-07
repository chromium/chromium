// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NodeStore} from '../content/node_store.js';
import {isRectVisible} from '../shared/common.js';

import {ReadAloudNodeStore} from './read_aloud_node_store.js';
import type {ReadAloudNode, Segment} from './read_aloud_types.js';
import {isInvalidHighlightForWordHighlighting} from './speech_presentation_rules.js';

export const previousReadHighlightClass = 'previous-read-highlight';
export const currentReadHighlightClass = 'current-read-highlight';
export const PARENT_OF_HIGHLIGHT_CLASS = 'parent-of-highlight';

// Represents a single granularity for moving forward and backward. A single
// granularity always represents a sentence right now, and it can consist of
// one or more Highlight objects.
export class MovementGranularity {
  private highlights_: Highlight[] = [];

  addHighlight(highlight: Highlight) {
    this.highlights_.push(highlight);
  }

  isEmpty() {
    return this.highlights_.length === 0 ||
        this.highlights_.every(highlight => highlight.isEmpty());
  }

  onWillHighlightWordOrPhrase(segmentsToHighlight: Segment[]) {
    // When switching from sentence highlight to word or phrase highlight, clear
    // that sentence highlight. This can happen when the user switches highlight
    // granularities or when the granularity is only now determined after
    // already highlighting a sentence.
    if (this.highlights_.length === 1 &&
        this.highlights_[0] instanceof SentenceHighlight) {
      const highlight = this.highlights_[0];
      this.clearFormatting();
      this.highlights_.pop();
      if (!segmentsToHighlight.length) {
        return;
      }
      // If the removed sentence highlight had multiple segments, and the next
      // node to be highlighted is not in the first segment, highlight the
      // segments leading up to that node.
      const segmentIndex =
          highlight.getSegmentIndexWithSameNode(segmentsToHighlight[0]);
      if (segmentIndex > 0) {
        const newSegments = highlight.getSegments().slice(0, segmentIndex);
        const newHighlight = new SentenceHighlight(newSegments);
        newHighlight.setPrevious();
        this.addHighlight(newHighlight);
      }
    } else {
      this.setPrevious();
    }
  }

  setPrevious() {
    this.highlights_.forEach(highlight => highlight.setPrevious());
  }

  clearFormatting() {
    this.highlights_.forEach(highlight => highlight.clearFormatting());
  }

  isVisible() {
    return isRectVisible(this.getBounds_());
  }

  scrollIntoView() {
    // Ensure all the current highlights are in view.
    // TODO: crbug.com/40927698 - Handle if the highlight is longer than the
    // full height of the window (e.g. when font size is very large). Possibly
    // using word boundaries to know when we've reached the bottom of the
    // window and need to scroll so the rest of the current highlight is
    // showing.
    const firstHighlight = this.getHighlightElements_().at(0);
    if (!firstHighlight) {
      return;
    }

    const highlightBounds = this.getBounds_();
    if (highlightBounds.height > (window.innerHeight / 2)) {
      // If the bottom of the highlight would be offscreen if we center it,
      // scroll the first highlight to the top instead of centering it.
      firstHighlight.scrollIntoView({block: 'start'});
    } else if (
        (highlightBounds.bottom > window.innerHeight) ||
        (highlightBounds.top < 0)) {
      // Otherwise center the current highlight if part of it would be cut
      // off.
      firstHighlight.scrollIntoView({block: 'center'});
    }
  }

  private getBounds_(): DOMRect {
    const bounds = new DOMRect();
    const currentHighlights = this.getHighlightElements_();
    if (!currentHighlights || !currentHighlights.length) {
      return bounds;
    }
    const firstHighlight = currentHighlights.at(0);
    const lastHighlight = currentHighlights.at(-1);
    if (!firstHighlight || !lastHighlight) {
      return bounds;
    }
    const firstRect = firstHighlight.getBoundingClientRect();
    const lastRect = lastHighlight.getBoundingClientRect();
    bounds.x = Math.min(firstRect.x, lastRect.x);
    bounds.y = firstRect.y;
    bounds.width = Math.max(firstRect.right, lastRect.right) - bounds.x;
    bounds.height = lastRect.bottom - firstRect.y;
    return bounds;
  }

  private getHighlightElements_(): HTMLElement[] {
    return this.highlights_.flatMap(highlight => highlight.getElements());
  }
}

// A class that represents a single logical highlight. Handles the DOM
// manipulation for one highlight block.
export abstract class Highlight {
  // The spans that are actually colored for current or previous highlighting.
  protected readonly highlightSpans_: HTMLElement[] = [];

  private readonly segments_: Segment[];

  protected nodeStore_: NodeStore = NodeStore.getInstance();

  constructor(segments: Segment[]) {
    this.segments_ = segments;
  }

  // Highlights the text in the given node from the start index to the end
  // index. If skipNonWords is true, this will not highlight text that is only
  // punctuation or whitespace.
  protected highlightNode_(
      node: ReadAloudNode, start: number, length: number,
      skipNonWords: boolean) {
    const element = node.domNode() as HTMLElement;
    if (!element || start < 0 || length <= 0) {
      return;
    }
    const end = start + length;
    let previousHighlightOnly = false;
    if (skipNonWords) {
      const textContent = element.textContent?.substring(start, end).trim();
      // If the text is just punctuation or whitespace, don't show it as a
      // current highlight, but do fade it out as 'before the current
      // highlight.'
      previousHighlightOnly =
          isInvalidHighlightForWordHighlighting(textContent);
    }
    const highlighted =
        this.highlightCurrentText_(start, end, element, previousHighlightOnly);
    if (highlighted) {
      this.nodeStore_.replaceDomNode(element, highlighted);

      // This could be grouped into NodeStore but is being handled as a
      // separate call to avoid moving more logic outside of the read_aloud/
      // directory.
      ReadAloudNodeStore.getInstance().update(element, highlighted);
    }
  }

  // The following results in
  // <span>
  //   <span class="previous-read-highlight"> prefix text </span>
  //   <span class="current-read-highlight"> highlighted text </span>
  //   suffix text
  // </span>
  protected highlightCurrentText_(
      highlightStart: number, highlightEnd: number, currentNode: HTMLElement,
      previousHighlightOnly: boolean = false): HTMLElement {
    const parentOfHighlight = document.createElement('span');
    parentOfHighlight.classList.add(PARENT_OF_HIGHLIGHT_CLASS);

    // First pull out any text within this node before the highlighted
    // section. Since it's already been highlighted, we fade it out.
    const highlightPrefix =
        currentNode.textContent.substring(0, highlightStart);
    if (highlightPrefix.length > 0) {
      const previousHighlight = document.createElement('span');
      previousHighlight.classList.add(previousReadHighlightClass);
      const prefixTextNode = document.createTextNode(highlightPrefix);
      previousHighlight.appendChild(prefixTextNode);
      this.nodeStore_.setAncestor(prefixTextNode, parentOfHighlight, 0);
      parentOfHighlight.appendChild(previousHighlight);
      this.highlightSpans_.push(previousHighlight);
    }

    // Then get the section of text to highlight and mark it for
    // highlighting.
    const readingHighlight = document.createElement('span');
    // In the case where we don't actually want to show the current highlight,
    // but the text should still be included in 'previously read', add the
    // previous formatting instead of the current formatting.
    if (previousHighlightOnly) {
      readingHighlight.classList.add(previousReadHighlightClass);
    } else {
      readingHighlight.classList.add(currentReadHighlightClass);
    }
    const textNode = document.createTextNode(
        currentNode.textContent.substring(highlightStart, highlightEnd));
    readingHighlight.appendChild(textNode);
    this.nodeStore_.setAncestor(textNode, parentOfHighlight, highlightStart);
    parentOfHighlight.appendChild(readingHighlight);
    this.highlightSpans_.push(readingHighlight);

    // Finally, append the rest of the text for this node that has yet to be
    // highlighted.
    const highlightSuffix = currentNode.textContent.substring(highlightEnd);
    if (highlightSuffix.length > 0) {
      const suffixNode = document.createTextNode(highlightSuffix);
      this.nodeStore_.setAncestor(suffixNode, parentOfHighlight, highlightEnd);
      parentOfHighlight.appendChild(suffixNode);
    }

    return parentOfHighlight;
  }

  getSegmentIndexWithSameNode(segmentToGet?: Segment) {
    if (!segmentToGet) {
      return -1;
    }
    const segment =
        this.segments_.filter(segment => segment.node.equals(segmentToGet.node))
            .at(0);
    return segment ? this.segments_.indexOf(segment) : -1;
  }

  setPrevious() {
    this.highlightSpans_.forEach(element => {
      element.classList.remove(currentReadHighlightClass);
      element.classList.add(previousReadHighlightClass);
    });
  }

  clearFormatting() {
    this.highlightSpans_.forEach(element => {
      element.classList.remove(previousReadHighlightClass);
      element.classList.remove(currentReadHighlightClass);
    });
  }

  isEmpty(): boolean {
    return this.highlightSpans_.length === 0;
  }

  getElements(): HTMLElement[] {
    return this.highlightSpans_;
  }

  getSegments(): Segment[] {
    return this.segments_;
  }
}

export class SentenceHighlight extends Highlight {
  constructor(segments: Segment[]) {
    super(segments);

    for (const {node, start, length} of segments) {
      this.highlightNode_(node, start, length, /*skipNonWords=*/ false);
    }
  }
}

export class WordHighlight extends Highlight {
  constructor(segments: Segment[], ttsWordLength: number) {
    super(segments);

    let accumulatedHighlightLength = 0;
    for (const {node, start, length: segmentLength} of segments) {
      // Prioritize the word boundary received from the TTS engine if there is
      // one.
      const useTtsWordLength = ttsWordLength > 0;
      const remainingTtsLength =
          Math.max(ttsWordLength - accumulatedHighlightLength, 0);
      const highlightLength =
          useTtsWordLength ? remainingTtsLength : segmentLength;

      this.highlightNode_(node, start, highlightLength, /*skipNonWords=*/ true);

      // Keep track of the highlight length that's been spoken so that
      // speechUtteranceLength can be used across multiple nodes.
      accumulatedHighlightLength += highlightLength;
    }
  }
}

export class PhraseHighlight extends Highlight {
  constructor(segments: Segment[]) {
    super(segments);

    for (const {node, start, length} of segments) {
      this.highlightNode_(node, start, length, /*skipNonWords=*/ true);
    }
  }
}
