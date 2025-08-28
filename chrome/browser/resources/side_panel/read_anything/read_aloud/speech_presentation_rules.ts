// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Characters that should be ignored for word highlighting when not accompanied
// by other characters.
const IGNORED_HIGHLIGHT_CHARACTERS_REGEX: RegExp = /^[.,!?'"(){}\[\]]+$/;

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
