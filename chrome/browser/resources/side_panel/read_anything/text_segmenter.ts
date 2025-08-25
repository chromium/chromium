// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Wrapper class for Intl.Segmenter that manages Intl.Segmenter instances to
// be used to segment text.
export class TextSegmenter {
  private wordSegmenter_!: Intl.Segmenter;

  constructor() {
    // If no language code has been provided, Intl.Segmenter will use the system
    // default language.
    // TODO: crbug.com/440400392- Use the page language to improve segmentation.
    this.updateLanguage();
  }

  updateLanguage(lang?: string) {
    // The try-catch is needed because Intl.Segmenter throws an error if the
    // language code is not well-formed.
    try {
      this.wordSegmenter_ = new Intl.Segmenter(lang, {granularity: 'word'});
    } catch {
      this.wordSegmenter_ =
          new Intl.Segmenter(undefined, {granularity: 'word'});
    }
  }

  getWordCount(text: string): number {
    return Array.from(this.wordSegmenter_.segment(text))
        .filter(segment => segment.isWordLike)
        .length;
  }

  static getInstance(): TextSegmenter {
    return instance || (instance = new TextSegmenter());
  }

  static setInstance(obj: TextSegmenter) {
    instance = obj;
  }
}

let instance: TextSegmenter|null = null;
