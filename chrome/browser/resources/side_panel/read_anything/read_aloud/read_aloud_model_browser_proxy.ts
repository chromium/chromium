// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ReadAloudNode, Segment} from './read_aloud_types.js';
import {TsReadModelImpl} from './ts_model_impl.js';

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

  // Requests phrase boundaries for the current sentence and the next few
  // sentences, so they're ready by the time they're highlighted. Optional
  // because the V8 model handles this in C++.
  prefetchPhraseBoundaries?(): void;
}

export function getReadAloudModel(): ReadAloudModelBrowserProxy {
  // TODO(crbug.com/440400392): Delete V8ModelImpl now that it's unused.
  return instance || (instance = new TsReadModelImpl());
}

export function setInstance(obj: ReadAloudModelBrowserProxy|null) {
  instance = obj;
}

let instance: ReadAloudModelBrowserProxy|null = null;
