// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import {MovementGranularity, PhraseHighlight, SentenceHighlight, WordHighlight} from './movement.js';
import type {Highlight} from './movement.js';
import {getReadAloudModel} from './read_aloud_model_browser_proxy.js';
import type {ReadAloudModelBrowserProxy} from './read_aloud_model_browser_proxy.js';
import type {Segment} from './read_aloud_types.js';
import {getCurrentSpeechRate} from './speech_presentation_rules.js';
import {VoiceLanguageController} from './voice_language_controller.js';
import {isEspeak} from './voice_language_conversions.js';
import {WordBoundaries} from './word_boundaries.js';

// Manages state and drawing of visual highlights for read aloud.
export class ReadAloudHighlighter {
  private previousGranularities_: MovementGranularity[] = [];
  private currentGranularity_: MovementGranularity|null = null;
  private wordBoundaries_: WordBoundaries = WordBoundaries.getInstance();
  private allowAutoScroll_ = true;
  private voiceLanguageController_ = VoiceLanguageController.getInstance();
  private readAloudModel_: ReadAloudModelBrowserProxy = getReadAloudModel();

  hasCurrentGranularity(): boolean {
    return !!this.currentGranularity_;
  }

  updateAutoScroll(): void {
    this.allowAutoScroll_ =
        !!this.currentGranularity_ && this.currentGranularity_.isVisible();
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
    if (highlightGranularity !== chrome.readingMode.sentenceHighlighting) {
      // When we're about to move to the next granularity, ensure the rest of
      // the sentence we are about to skip is still highlighted with previous
      // highlight formatting.
      this.highlightCurrentSentence_(
          segments, /*scrollIntoView=*/ false,
          /* previousHighlightOnly=*/ true);
    }


    if (this.currentGranularity_) {
      this.currentGranularity_.setPrevious();
      this.previousGranularities_.push(this.currentGranularity_);
      this.currentGranularity_ = null;
    }
  }

  onWillMoveToPreviousGranularity() {
    if (this.currentGranularity_) {
      this.currentGranularity_.clearFormatting();
      this.currentGranularity_ = null;
      const lastPrevious = this.previousGranularities_.pop();
      lastPrevious?.clearFormatting();
    }
  }

  // Restores the highlight formatting to the existing previous granularity
  // queue if there is one.
  restorePreviousHighlighting() {
    this.previousGranularities_.forEach(highlight => highlight.setPrevious());
  }

  // Removes all highlight formatting, but maintains the previous granularity
  // queue.
  clearHighlightFormatting() {
    if (this.currentGranularity_) {
      this.currentGranularity_.clearFormatting();
      this.currentGranularity_ = null;
    }
    this.previousGranularities_.forEach(
        highlight => highlight.clearFormatting());
  }

  // Clears all the formatting and discards the granularity queue.
  reset() {
    this.clearHighlightFormatting();
    this.previousGranularities_ = [];
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

    // Until ts_model_impl supports phrase highlighting, always fallback to
    // sentence highlighting.

    const currentSpeechRate: number = getCurrentSpeechRate();

    if (!chrome.readingMode.isPhraseHighlightingEnabled ||
        chrome.readingMode.isTsTextSegmentationEnabled) {
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
    const {
      speechUtteranceStartIndex,
      previouslySpokenIndex,
      speechUtteranceLength,
    } = this.wordBoundaries_.state;
    const index = speechUtteranceStartIndex + previouslySpokenIndex;
    const highlightSegments =
        this.readAloudModel_.getHighlightForCurrentSegmentIndex(
            index, highlightPhrases);

    if (this.currentGranularity_) {
      this.currentGranularity_.onWillHighlightWordOrPhrase(highlightSegments);
    }
    const highlight = highlightPhrases ?
        new PhraseHighlight(highlightSegments) :
        new WordHighlight(highlightSegments, speechUtteranceLength);
    if (!highlight.isEmpty()) {
      this.addHighlightToCurrentGranularity_(highlight);
      this.scrollHighlightIntoView_();
    }
  }

  private highlightCurrentSentence_(
      segments: Segment[], scrollIntoView: boolean,
      previousHighlightOnly: boolean = false) {
    if (!segments.length) {
      return;
    }

    const highlight = new SentenceHighlight(segments);
    if (previousHighlightOnly) {
      highlight.setPrevious();
    }
    this.addHighlightToCurrentGranularity_(highlight);
    if (scrollIntoView) {
      this.scrollHighlightIntoView_();
    }
  }

  private addHighlightToCurrentGranularity_(highlight: Highlight) {
    if (highlight.isEmpty()) {
      return;
    }

    // If there's no current granularity yet, it means the last granularity is
    // finished (or this is the first granularity), so create a new one.
    // Otherwise, the given highlight is likely a word or phrase that is less
    // than a full granularity on its own, so add it to the existing
    // granularity.
    if (!this.currentGranularity_) {
      this.currentGranularity_ = new MovementGranularity();
    }
    this.currentGranularity_.addHighlight(highlight);
  }

  private scrollHighlightIntoView_() {
    if (!this.allowAutoScroll_) {
      this.updateAutoScroll();
      if (!this.allowAutoScroll_) {
        return;
      }
    }


    this.currentGranularity_?.scrollIntoView();
  }

  static getInstance(): ReadAloudHighlighter {
    return instance || (instance = new ReadAloudHighlighter());
  }

  static setInstance(obj: ReadAloudHighlighter) {
    instance = obj;
  }
}

let instance: ReadAloudHighlighter|null = null;
