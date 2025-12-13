// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

enum WordBoundaryMode {
  // Used if word boundaries are not supported (i.e. we haven't received enough
  // information to determine if word boundaries are supported.)
  BOUNDARIES_NOT_SUPPORTED,
  NO_BOUNDARIES,
  BOUNDARY_DETECTED,
}

export interface WordBoundaryState {
  mode: WordBoundaryMode;
  // The charIndex of the last word boundary index retrieved by the "Boundary"
  // event. Default is 0.
  previouslySpokenIndex: number;
  // Is only non-zero if the current state has already resumed speech on a
  // word boundary. e.g. If we interrupted speech for the segment
  // "This is a sentence" at "is," so the next segment spoken is "is a
  // sentence," if we attempt to interrupt speech again at "a." This helps us
  // keep track of the correct index in the overall granularity string- not
  // just the correct index within the current string.
  // Default is 0.
  speechUtteranceStartIndex: number;
  // The length of the current word if it was provided by the speech engine. If
  // not, this is 0.
  speechUtteranceLength: number;
}

export class WordBoundaries {
  state: WordBoundaryState = {
    mode: WordBoundaryMode.BOUNDARIES_NOT_SUPPORTED,
    speechUtteranceStartIndex: 0,
    previouslySpokenIndex: 0,
    speechUtteranceLength: 0,
  };

  hasBoundaries(): boolean {
    return this.state.mode === WordBoundaryMode.BOUNDARY_DETECTED;
  }

  notSupported(): boolean {
    return this.state.mode === WordBoundaryMode.BOUNDARIES_NOT_SUPPORTED;
  }

  /**
   * Resets the state to a default configuration.
   *
   * If a word boundary was previously detected, the mode is set to
   * NO_BOUNDARIES. This is because we know boundaries are supported and are
   * simply clearing the current state. Otherwise, the mode is set to
   * BOUNDARIES_NOT_SUPPORTED.
   */
  resetToDefaultState() {
    const defaultMode =
        (this.state.mode === WordBoundaryMode.BOUNDARY_DETECTED) ?
        WordBoundaryMode.NO_BOUNDARIES :
        WordBoundaryMode.BOUNDARIES_NOT_SUPPORTED;
    this.state = {
      previouslySpokenIndex: 0,
      mode: defaultMode,
      speechUtteranceStartIndex: 0,
      speechUtteranceLength: 0,
    };
  }

  /**
   * Explicitly sets the mode to indicate that word boundaries are not or might
   * not be supported.
   */
  setNotSupported(): void {
    this.state.mode = WordBoundaryMode.BOUNDARIES_NOT_SUPPORTED;
  }

  // Returns the index of the word boundary at which we had previously paused.
  getResumeBoundary(): number {
    const substringIndex =
        this.state.previouslySpokenIndex + this.state.speechUtteranceStartIndex;
    this.state.previouslySpokenIndex = 0;
    this.state.speechUtteranceStartIndex = substringIndex;
    return substringIndex;
  }

  updateBoundary(charIndex: number, charLength: number = 0) {
    this.state.previouslySpokenIndex = charIndex;
    this.state.mode = WordBoundaryMode.BOUNDARY_DETECTED;
    this.state.speechUtteranceLength = charLength;
  }

  static getInstance(): WordBoundaries {
    return instance || (instance = new WordBoundaries());
  }

  static setInstance(obj: WordBoundaries) {
    instance = obj;
  }
}

let instance: WordBoundaries|null = null;
