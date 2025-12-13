// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Characters that should be ignored for word highlighting when not accompanied
// by other characters.
const IGNORED_HIGHLIGHT_CHARACTERS_REGEX: RegExp = /^[.,!?'"(){}\[\]]+$/;

// Opening punctuation characters that text segmentation should not end a
// sentence on if there's a valid sentence after the current sentence. This
// reduces the risk of opening punctuation characters being read out loud
// by read aloud.
const OPENING_PUNCTUATION_CHARACTERS_REGEX: RegExp = /[({<[]+$/;

export function getCurrentSpeechRate(): number {
  return parseFloat(chrome.readingMode.speechRate.toFixed(1));
}

// If a highlight is just white space or punctuation, we can skip
// highlighting.
export function isInvalidHighlightForWordHighlighting(textToHighlight?: string):
    boolean {
  const text = textToHighlight?.trim();
  return !text || text === '' || IGNORED_HIGHLIGHT_CHARACTERS_REGEX.test(text);
}

// If the given text ends with opening punctuation.
export function textEndsWithOpeningPunctuation(text: string): RegExpMatchArray|
    null {
  return text.match(OPENING_PUNCTUATION_CHARACTERS_REGEX);
}
