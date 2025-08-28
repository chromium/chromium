// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import {isRectVisible} from '../common.js';
import {NodeStore} from '../node_store.js';

import {getReadAloudModel} from './read_aloud_model_browser_proxy.js';
import type {ReadAloudModelBrowserProxy} from './read_aloud_model_browser_proxy.js';
import {AxReadAloudNode} from './read_aloud_types.js';
import type {Segment} from './read_aloud_types.js';
import {getCurrentSpeechRate, isInvalidHighlightForWordHighlighting} from './speech_presentation_rules.js';
import {VoiceLanguageController} from './voice_language_controller.js';
import {isEspeak} from './voice_language_conversions.js';
import {WordBoundaries} from './word_boundaries.js';

export const previousReadHighlightClass = 'previous-read-highlight';
export const currentReadHighlightClass = 'current-read-highlight';
const PARENT_OF_HIGHLIGHT_CLASS = 'parent-of-highlight';

// Manages state and drawing of visual highlights for read aloud.
export class ReadAloudHighlighter {
  private previousHighlights_: HTMLElement[] = [];
  // Key: a DOM node that's already been read aloud
  // Value: the index offset at which this node's text begins within its parent
  // text. For reading aloud we sometimes split up nodes so the speech sounds
  // more natural. When that text is then selected we need to pass the correct
  // index down the pipeline, so we store that info here.
  private highlightedNodeToOffsetInParent_: Map<Node, number> = new Map();
  private wordBoundaries_: WordBoundaries = WordBoundaries.getInstance();
  private nodeStore_: NodeStore = NodeStore.getInstance();
  private allowAutoScroll_ = true;
  private voiceLanguageController_ = VoiceLanguageController.getInstance();
  private readAloudModel_: ReadAloudModelBrowserProxy = getReadAloudModel();

  hasCurrentHighlights(): boolean {
    return this.previousHighlights_.some(
        highlight => highlight.classList.contains(currentReadHighlightClass));
  }

  private getCurrentHighlights_(): HTMLElement[] {
    return this.previousHighlights_.filter(
        highlight => highlight.classList.contains(currentReadHighlightClass));
  }

  updateAutoScroll(): void {
    this.allowAutoScroll_ = isRectVisible(this.getCurrentHighlightBounds_());
  }

  getOffsetInAncestor(node: Node): number {
    if (this.highlightedNodeToOffsetInParent_.has(node)) {
      return this.highlightedNodeToOffsetInParent_.get(node)!;
    }

    return 0;
  }

  getAncestorId(node: Node): number|undefined {
    const ancestor = this.getAncestor_(node);
    return ancestor ? this.nodeStore_.getAxId(ancestor) : undefined;
  }

  highlightCurrentGranularity(
      segments: Segment[], scrollIntoView: boolean,
      shouldUpdateSentenceHighlight: boolean): void {
    const highlightGranularity = this.getEffectiveHighlightingGranularity_();
    switch (highlightGranularity) {
      case chrome.readingMode.noHighlighting:
      // Even without highlighting, we may still need to calculate the sentence
      // highlight, so that it's visible as soon as the user turns on sentence
      // highlighting. The highlight will not be visible, since the highlight
      // color in this case will be transparent. However, we don't need to
      // recalculate the sentence highlights sometimes, such as during word
      // boundary events when sentence highlighting is used, since these
      // highlights have already been calculated.
      case chrome.readingMode.sentenceHighlighting:
        if (shouldUpdateSentenceHighlight) {
          this.highlightCurrentSentence_(segments, scrollIntoView);
        }
        break;
      case chrome.readingMode.wordHighlighting:
        this.highlightCurrentWordOrPhrase_(/*highlightPhrases=*/ false);
        break;
      case chrome.readingMode.phraseHighlighting:
        this.highlightCurrentWordOrPhrase_(/*highlightPhrases=*/ true);
        break;
      case chrome.readingMode.autoHighlighting:
      default:
        // This cannot happen, but ensures the switch statement is exhaustive.
        assert(false, 'invalid value for effective highlight');
    }
  }

  onWillMoveToNextGranularity(segments: Segment[]) {
    const highlightGranularity = this.getEffectiveHighlightingGranularity_();
    if (highlightGranularity === chrome.readingMode.sentenceHighlighting) {
      return;
    }

    // When we're about to move to the next granularity, ensure the rest of the
    // sentence we are about to skip is still highlighted for previous
    // highlight formatting.
    this.highlightCurrentSentence_(
        segments, /*scrollIntoView=*/ false,
        /* previousHighlightOnly=*/ true);
  }

  // Resets formatting on the current highlight, including previous highlight
  // formatting.
  removeCurrentHighlight(segments: Segment[]) {
    // The most recent highlight could have been spread across multiple
    // segments so clear the formatting for all of the segments.
    for (let i = 0; i < segments.length; i++) {
      const lastElement = this.previousHighlights_.pop();
      if (lastElement) {
        lastElement.classList.remove(currentReadHighlightClass);
      }
    }
  }

  // Sets the previous highlight formatting and removes the current highlight
  // formatting.
  resetPreviousHighlight() {
    this.previousHighlights_.forEach((element) => {
      if (element) {
        element.classList.add(previousReadHighlightClass);
        element.classList.remove(currentReadHighlightClass);
      }
    });
  }

  // Removes all highlight formatting.
  clearHighlightFormatting() {
    this.previousHighlights_.forEach((element) => {
      if (element) {
        element.classList.remove(previousReadHighlightClass);
        element.classList.remove(currentReadHighlightClass);
      }
    });
    this.previousHighlights_ = [];
  }

  private getCurrentHighlightBounds_(): DOMRect {
    const bounds = new DOMRect();
    const currentHighlights = this.getCurrentHighlights_();
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

  private getAncestor_(node: Node): Node|null {
    if (!node.parentElement || !node.parentNode) {
      return null;
    }

    let ancestor = null;
    if (node.parentElement.classList.contains(PARENT_OF_HIGHLIGHT_CLASS)) {
      ancestor = node.parentNode;
    } else if (node.parentElement.parentElement?.classList.contains(
                   PARENT_OF_HIGHLIGHT_CLASS)) {
      ancestor = node.parentNode.parentNode;
    }

    return ancestor;
  }

  private getEffectiveHighlightingGranularity_(): number {
    // Parse all of the conditions that control highlighting and return the
    // effective highlighting granularity.
    const highlight = chrome.readingMode.highlightGranularity;

    if (highlight === chrome.readingMode.noHighlighting ||
        highlight === chrome.readingMode.sentenceHighlighting) {
      return highlight;
    }

    if (this.wordBoundaries_.notSupported() ||
        isEspeak(this.voiceLanguageController_.getCurrentVoice())) {
      // Fall back where word highlighting is not possible. Since espeak
      // boundaries are different than Google TTS word boundaries, fall back
      // to sentence boundaries in that case too.
      return chrome.readingMode.sentenceHighlighting;
    }

    const currentSpeechRate: number = getCurrentSpeechRate();

    if (!chrome.readingMode.isPhraseHighlightingEnabled) {
      // Choose sentence highlighting for fast voices.
      if (currentSpeechRate > 1.2 &&
          highlight === chrome.readingMode.autoHighlighting) {
        return chrome.readingMode.sentenceHighlighting;
      }

      // In other cases where phrase highilghting is off, choose word
      // highlighting.
      return chrome.readingMode.wordHighlighting;
    }

    // TODO: crbug.com/364327601 - Check that the language of the page should
    // be English for phrase highlighting.
    if (highlight === chrome.readingMode.autoHighlighting) {
      if (currentSpeechRate <= 0.8) {
        return chrome.readingMode.wordHighlighting;
      } else if (currentSpeechRate >= 2.0) {
        return chrome.readingMode.sentenceHighlighting;
      } else {
        return chrome.readingMode.phraseHighlighting;
      }
    }

    // In other cases, return what the user selected (i.e. word/phrase).
    return highlight;
  }

  private highlightCurrentWordOrPhrase_(highlightPhrases: boolean): void {
    this.resetCurrentHighlight_();
    this.resetPreviousHighlight();
    const {
      speechUtteranceStartIndex,
      previouslySpokenIndex,
      speechUtteranceLength,
    } = this.wordBoundaries_.state;
    const index = speechUtteranceStartIndex + previouslySpokenIndex;
    const highlightSegments =
        this.readAloudModel_.getHighlightForCurrentSegmentIndex(
            index, highlightPhrases);
    let accumulatedHighlightLength = 0;
    let didApplyHighlight = false;
    for (const segment of highlightSegments) {
      const {node, start, length: segmentLength} = segment;
      if (!(node instanceof AxReadAloudNode)) {
        continue;
      }
      const nodeId = node.axNodeId;

      const domNode = this.nodeStore_.getDomNode(nodeId);
      if (!domNode) {
        continue;
      }


      // For phrase highlighting, always use the segment length received from
      // getHighlightForCurrentSegmentIndex. For word highlighting, prioritize
      // the word boundary received from the TTS engine if there is one.
      const useTtsWordLength = !highlightPhrases && speechUtteranceLength > 0;
      const remainingTtsLength =
          Math.max(speechUtteranceLength - accumulatedHighlightLength, 0);
      const highlightLength =
          useTtsWordLength ? remainingTtsLength : segmentLength;

      if (highlightLength <= 0) {
        continue;
      }

      const endIndex = start + highlightLength;
      const textContent =
          domNode.textContent?.substring(start, endIndex).trim();
      // If the remaining text is just punctuation, don't show it as a current
      // highlight, but do fade it out as 'before the current highlight.'
      const previousHighlightOnly =
          isInvalidHighlightForWordHighlighting(textContent);
      const element = domNode as HTMLElement;
      const highlightedNode = this.highlightCurrentText_(
          start, endIndex, element, previousHighlightOnly);
      this.nodeStore_.replaceDomNode(element, highlightedNode);

      // Keep track of the highlight length that's been spoken so that
      // speechUtteranceLength can be used across multiple nodes.
      accumulatedHighlightLength += highlightLength;
      didApplyHighlight = true;
    }

    if (didApplyHighlight) {
      this.scrollHighlightIntoView_();
    }
  }

  private highlightCurrentSentence_(
      segments: Segment[], scrollIntoView: boolean,
      previousHighlightOnly: boolean = false) {
    if (!segments.length) {
      return;
    }

    this.resetPreviousHighlight();
    for (const {node, start, length} of segments) {
      if (!(node instanceof AxReadAloudNode)) {
        continue;
      }
      const element = this.nodeStore_.getDomNode(node.axNodeId) as HTMLElement;
      if (!element) {
        continue;
      }
      const end = start + length;
      if (start < 0 || end < 0) {
        continue;
      }
      const highlighted = this.highlightCurrentText_(
          start, start + length, element, previousHighlightOnly);
      if (highlighted) {
        this.nodeStore_.replaceDomNode(element, highlighted);
      }
    }

    if (scrollIntoView) {
      this.scrollHighlightIntoView_();
    }
  }

  // The following results in
  // <span>
  //   <span class="previous-read-highlight"> prefix text </span>
  //   <span class="current-read-highlight"> highlighted text </span>
  //   suffix text
  // </span>
  private highlightCurrentText_(
      highlightStart: number, highlightEnd: number, currentNode: HTMLElement,
      previousHighlightOnly: boolean = false): HTMLElement {
    const parentOfHighlight = document.createElement('span');
    parentOfHighlight.classList.add(PARENT_OF_HIGHLIGHT_CLASS);

    // First pull out any text within this node before the highlighted
    // section. Since it's already been highlighted, we fade it out.
    const highlightPrefix =
        currentNode.textContent!.substring(0, highlightStart);
    if (highlightPrefix.length > 0) {
      const prefixNode = document.createElement('span');
      prefixNode.classList.add(previousReadHighlightClass);
      prefixNode.textContent = highlightPrefix;
      this.previousHighlights_.push(prefixNode);
      parentOfHighlight.appendChild(prefixNode);
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
        currentNode.textContent!.substring(highlightStart, highlightEnd));
    readingHighlight.appendChild(textNode);
    this.highlightedNodeToOffsetInParent_.set(textNode, highlightStart);
    parentOfHighlight.appendChild(readingHighlight);

    // Finally, append the rest of the text for this node that has yet to be
    // highlighted.
    const highlightSuffix = currentNode.textContent!.substring(highlightEnd);
    if (highlightSuffix.length > 0) {
      const suffixNode = document.createTextNode(highlightSuffix);
      this.highlightedNodeToOffsetInParent_.set(suffixNode, highlightEnd);
      parentOfHighlight.appendChild(suffixNode);
    }

    // Replace the current node in the tree with the split up version of the
    // node.
    this.previousHighlights_.push(readingHighlight);
    return parentOfHighlight;
  }

  private scrollHighlightIntoView_() {
    if (!this.allowAutoScroll_) {
      this.updateAutoScroll();
      if (!this.allowAutoScroll_) {
        return;
      }
    }


    // Ensure all the current highlights are in view.
    // TODO: crbug.com/40927698 - Handle if the highlight is longer than the
    // full height of the window (e.g. when font size is very large). Possibly
    // using word boundaries to know when we've reached the bottom of the
    // window and need to scroll so the rest of the current highlight is
    // showing.
    const firstHighlight = this.getCurrentHighlights_().at(0);
    if (!firstHighlight) {
      return;
    }
    const highlightBounds = this.getCurrentHighlightBounds_();
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

  // Resets the current highlight. Does not change how this element will
  // be considered for previous highlighting.
  private resetCurrentHighlight_() {
    this.previousHighlights_.forEach((element) => {
      if (element) {
        element.classList.remove(currentReadHighlightClass);
      }
    });
  }

  static getInstance(): ReadAloudHighlighter {
    return instance || (instance = new ReadAloudHighlighter());
  }

  static setInstance(obj: ReadAloudHighlighter) {
    instance = obj;
  }
}

let instance: ReadAloudHighlighter|null = null;
