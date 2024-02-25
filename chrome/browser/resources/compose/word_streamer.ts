// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

// Accepts text updates, chunks the text up, and provides the caller with
// updates to a list of chunks.
// Chunks content by whitespace, and limits the rate of chunks.
export class WordStreamer {
  private msPerTick_ = 100;
  private charsPerTick_ = 10;
  private msWaitBeforeComplete_ = 300;

  // All words provided to WordStreamer.
  private words_: string[] = [];
  private displayState_: DisplayState = new DisplayState();
  private nextTick_: number|undefined;
  private finalTick_: number|undefined;
  private finalTextReceived_ = false;

  constructor(public callback: (words: string[], isComplete: boolean) => void) {
  }

  // Stop any processing and reset to the initial state.
  reset() {
    if (this.nextTick_) {
      window.clearTimeout(this.nextTick_);
    }
    if (this.finalTick_) {
      window.clearTimeout(this.finalTick_);
    }
    this.words_ = [];
    this.displayState_ = new DisplayState();
    this.nextTick_ = undefined;
    this.finalTextReceived_ = false;
  }

  // Set the current text to chunk. isFinal should be true on the last call to
  // setText.
  setText(text: string, isFinal: boolean) {
    // Should receive isFinal=true once.
    assert(!isFinal || !this.finalTextReceived_);
    // Note that until isFinal==true, we don't want the last word to prevent
    // display of partial words.
    this.words_ = splitIntoWords(text, /*includeLast=*/ isFinal);

    // Verify that the already displayed words are still the beginning of the
    // provided text param. If the already displayed words and the new provided
    // text have diverged, restart from the point of diversion.
    if (this.displayState_.update(0, this.words_)) {
      this.callback([...this.displayState_.words], false);
    }

    this.finalTextReceived_ = isFinal;
    if (!this.nextTick_) {
      this.scheduleUpdate();
    }
  }

  private scheduleUpdate() {
    if (this.nextTick_) {
      window.clearTimeout(this.nextTick_);
    }
    this.nextTick_ = window.setTimeout(() => this.tick(), this.msPerTick_);
  }

  private tick() {
    this.nextTick_ = undefined;

    // Advance N characters per tick.
    const displayChanged =
        this.displayState_.update(this.charsPerTick_, this.words_);
    const pendingWordsToDisplay =
        this.displayState_.words.length !== this.words_.length;
    const finalStateDisplayed =
        this.finalTextReceived_ && !pendingWordsToDisplay;

    if (displayChanged) {
      this.callback([...this.displayState_.words], false);
    }
    if (pendingWordsToDisplay) {
      this.scheduleUpdate();
    } else {
      // Don't let pending characters build up if we're constrained on the input
      // side.
      this.displayState_.clearPendingChars();
    }
    if (finalStateDisplayed) {
      // Delay before transitioning to the completed ux.
      this.finalTick_ = window.setTimeout(() => {
        this.callback([...this.displayState_.words], true);
        this.finalTick_ = undefined;
      }, this.msWaitBeforeComplete_);
    }
  }
  setMsPerTickForTesting(msPerTick: number) {
    this.msPerTick_ = msPerTick;
  }
  setCharsPerTickForTesting(charsPerTick: number) {
    this.charsPerTick_ = charsPerTick;
  }
  setMsWaitBeforeCompleteForTesting(msWaitBeforeComplete: number) {
    this.msWaitBeforeComplete_ = msWaitBeforeComplete;
  }
}

// Splits `text` into space-delimited words. If `includeLast` is false,
// the last word is not returned.
function splitIntoWords(text: string, includeLast: boolean) {
  let words = text.split(' ');
  if (words.length > 0 && !includeLast) {
    words.pop();
  }
  // ''.split(' ') --> [""], but we want []
  if (words.length === 1 && words[0] === '') {
    words = [];
  }
  return words.map((s, i) => (i > 0 ? ' ' : '') + s);
}

// Owns and calculates the words that should be displayed.
class DisplayState {
  // The displayed words.
  private words_: string[] = [];
  // The number of additional characters that should be displayed.
  private pendingChars_: number = 0;

  clearPendingChars() {
    this.pendingChars_ = 0;
  }

  // Updates the state. `additionalChars` is the additional number of characters
  // that should be displayed. `allWords` is the list of all words that should
  // be eventually displayed. `this.words()` will eventually converge to be
  // equal to `allWords`. Returns true if displayed words has changed.
  update(additionalChars: number, allWords: string[]): boolean {
    this.pendingChars_ += additionalChars;
    let modified = false;

    // Truncate `words_` if it's not a prefix of allWords.
    let prefixLen = 0;
    while (prefixLen < Math.min(allWords.length, this.words_.length) &&
           allWords[prefixLen] === this.words_[prefixLen]) {
      ++prefixLen;
    }
    if (prefixLen !== this.words_.length) {
      modified = true;
      this.words_.splice(prefixLen);
    }

    // Append words.
    while (allWords.length > this.words_.length) {
      const nextWord = allWords[this.words_.length];
      if (nextWord.length > this.pendingChars_) {
        break;
      }
      this.words_.push(nextWord);
      this.pendingChars_ -= nextWord.length;
      modified = true;
    }
    return modified;
  }

  // Words that should be displayed.
  get words(): string[] {
    return this.words_;
  }
}
