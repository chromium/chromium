// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import {getCurrentSpeechRate, isRectVisible} from '../common.js';
import {NodeStore} from '../node_store.js';
import {isEspeak} from '../voice_language_util.js';

import {VoiceLanguageController} from './voice_language_controller.js';
import {WordBoundaries} from './word_boundaries.js';

// Characters that should be ignored for word highlighting when not accompanied
// by other characters.
const IGNORED_HIGHLIGHT_CHARACTERS_REGEX: RegExp = /^[.,!?'"(){}\[\]]+$/;

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
      axNodeIds: number[], scrollIntoView: boolean,
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
          this.highlightCurrentSentence_(axNodeIds, scrollIntoView);
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

  // Resets formatting on the current highlight, including previous highlight
  // formatting.
  removeCurrentHighlight() {
    // The most recent highlight could have been spread across multiple
    // segments so clear the formatting for all of the segments.
    for (let i = 0; i < chrome.readingMode.getCurrentText().length; i++) {
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


  isInvalidHighlightForWordHighlighting(textToHighlight?: string): boolean {
    // If a highlight is just white space or punctuation, we can skip
    // highlighting.
    const text = textToHighlight?.trim();
    return !text || text === '' ||
        IGNORED_HIGHLIGHT_CHARACTERS_REGEX.test(text);
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
    bounds.x = firstHighlight.getBoundingClientRect().x;
    bounds.y = lastHighlight.getBoundingClientRect().y;
    bounds.width = Math.max(
        firstHighlight.getBoundingClientRect().width,
        lastHighlight.getBoundingClientRect().width);
    bounds.height = Math.max(
        firstHighlight.getBoundingClientRect().height,
        lastHighlight.getBoundingClientRect().height);
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
    const wordBoundaryState = this.wordBoundaries_.state;
    const index = wordBoundaryState.speechUtteranceStartIndex +
        wordBoundaryState.previouslySpokenIndex;
    const speechUtteranceLength = wordBoundaryState.speechUtteranceLength;
    let alreadyHighlightedSpeechUtteranceLength = 0;

    const highlightNodes =
        chrome.readingMode.getHighlightForCurrentSegmentIndex(
            index, highlightPhrases);
    let hasHighlights = false;
    for (const highlightNode of highlightNodes) {
      const nodeId = highlightNode.nodeId;
      const remainingSpeechUtteranceLength = Math.max(
          speechUtteranceLength - alreadyHighlightedSpeechUtteranceLength, 0);
      const highlightLength: number = speechUtteranceLength ?
          (remainingSpeechUtteranceLength) :
          highlightNode.length;
      const highlightStartIndex = highlightNode.start;
      const endIndex = highlightStartIndex + highlightLength;
      const node = this.nodeStore_.getDomNode(nodeId);

      if (!node) {
        continue;
      }
      const currentText =
          node.textContent?.substring(highlightStartIndex, endIndex).trim();
      if (this.isInvalidHighlightForWordHighlighting(currentText)) {
        continue;
      }

      // Keep track of the highlight length that's been spoken so that
      // speechUtteranceLength can be used across multiple nodes.
      alreadyHighlightedSpeechUtteranceLength += highlightLength;

      hasHighlights = true;
      const element = node as HTMLElement;
      const highlighted =
          this.highlightCurrentText_(highlightStartIndex, endIndex, element);
      this.nodeStore_.replaceDomNode(element, highlighted);
    }

    if (hasHighlights) {
      this.scrollHighlightIntoView_();
    }
  }

  private highlightCurrentSentence_(
      nodeIds: number[], scrollIntoView: boolean) {
    if (!nodeIds.length) {
      return;
    }

    this.resetPreviousHighlight();
    for (const nodeId of nodeIds) {
      const element = this.nodeStore_.getDomNode(nodeId) as HTMLElement;
      const highlighted = this.highlightCurrentElement_(nodeId, element);
      if (highlighted) {
        this.nodeStore_.replaceDomNode(element, highlighted);
      }
    }

    if (scrollIntoView) {
      this.scrollHighlightIntoView_();
    }
  }

  private highlightCurrentElement_(nodeId: number, element?: HTMLElement):
      HTMLElement|undefined {
    if (!element) {
      return undefined;
    }
    const start = chrome.readingMode.getCurrentTextStartIndex(nodeId);
    const end = chrome.readingMode.getCurrentTextEndIndex(nodeId);
    if ((start < 0) || (end < 0)) {
      // If the start or end index is invalid, don't use this node.
      return undefined;
    }
    return this.highlightCurrentText_(start, end, element);
  }

  // The following results in
  // <span>
  //   <span class="previous-read-highlight"> prefix text </span>
  //   <span class="current-read-highlight"> highlighted text </span>
  //   suffix text
  // </span>
  private highlightCurrentText_(
      highlightStart: number, highlightEnd: number,
      currentNode: HTMLElement): HTMLElement {
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
    readingHighlight.classList.add(currentReadHighlightClass);
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
