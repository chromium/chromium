// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ReadAloudNode, Segment} from './read_aloud_types.js';
import {TsReadModelImpl} from './ts_model_impl.js';
import {V8ModelImpl} from './v8_model_impl.js';

// TODO: crbug.com/440400392- Use TestReadAloudModelBrowserProxy to replace
// FakeReadingMode.

// Proxy class used to wrap text segmentation calls. This can be used to use
// different text segmentation approaches via feature flag, such as an
// implementation in C++ and an implementation in TypeScript.
export interface ReadAloudModelBrowserProxy {
  // TODO: crbug.com/440400392- Ensure all methods have documentation once
  // the structure is finalized.
  getHighlightForCurrentSegmentIndex(index: number, phrases: boolean):
      Segment[];
  getCurrentTextSegments(): Segment[];
  getCurrentTextContent(): string;

  // Returns a substring of the given text at the nearest accessible boundary
  // that is no longer than maxSpeechLength. This is used for handling text
  // that is too long the TTS engine to process at once. This is a workaround
  // for when remote voices are used because some remote voices hang instead of
  // returning a too-long-text error.
  getAccessibleText(text: string, maxSpeechLength: number): string;

  // Handle speech positioning.
  resetSpeechToBeginning(): void;
  moveSpeechForward(): void;
  moveSpeechBackwards(): void;

  // Handle initialization.
  isInitialized(): boolean;
  init(context: ReadAloudNode): void;

  // Resets the model. Method is optional because the V8 model handles this
  // in C++.
  resetModel?(): void;

  // Deletes a node from the model. Optional because the V8 model handles this
  // in C++.
  onNodeWillBeDeleted?(node: Node): void;
}

export function getReadAloudModel(): ReadAloudModelBrowserProxy {
  return instance ||
      (chrome.readingMode.isTsTextSegmentationEnabled ?
           instance = new TsReadModelImpl() :
           instance = new V8ModelImpl());
}

export function setInstance(obj: ReadAloudModelBrowserProxy|null) {
  instance = obj;
}

let instance: ReadAloudModelBrowserProxy|null = null;
