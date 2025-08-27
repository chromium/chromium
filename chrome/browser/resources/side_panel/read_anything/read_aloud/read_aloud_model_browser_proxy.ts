// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AxReadAloudNode} from './read_aloud_types.js';
import type {ReadAloudNode, Segment} from './read_aloud_types.js';
import {TextSegmenter} from './text_segmenter.js';

// TODO: crbug.com/440400392- Use TestReadAloudModelBrowserProxy to replace
// FakeReadingMode.

// TODO: crbug.com/440400392- Move logic into read_aloud_model. The browser
// proxy should only be a wrapper around browser calls (e.g.
// chrome.readingMode).

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
  getAccessibleBoundary(text: string, maxSpeechLength: number): number;

  // Handle speech positioning.
  resetSpeechToBeginning(): void;
  moveSpeechForward(): void;
  moveSpeechBackwards(): void;

  // Handle initialization.
  isInitialized(): boolean;
  init(context: ReadAloudNode|string): void;
}

class V8ModelImpl implements ReadAloudModelBrowserProxy {

  getHighlightForCurrentSegmentIndex(index: number, phrases: boolean):
      Segment[] {
    return chrome.readingMode.getHighlightForCurrentSegmentIndex(index, phrases)
        .map(
            ({nodeId, start, length}) =>
                ({node: new AxReadAloudNode(nodeId), start, length}));
  }

  getCurrentTextSegments():
      Segment[] {
    return chrome.readingMode.getCurrentTextSegments().map(
        ({nodeId, start, length}) =>
            ({node: new AxReadAloudNode(nodeId), start, length}));
  }

  getCurrentTextContent(): string {
    return chrome.readingMode.getCurrentTextContent();
  }

  resetSpeechToBeginning(): void {
    chrome.readingMode.resetGranularityIndex();
  }

  getAccessibleBoundary(text: string, maxSpeechLength: number): number {
    return chrome.readingMode.getAccessibleBoundary(text, maxSpeechLength);
  }

  isInitialized(): boolean {
    return chrome.readingMode.isSpeechTreeInitialized;
  }

  init(textNode: ReadAloudNode) {
    if (!(textNode instanceof AxReadAloudNode)) {
      return;
    }
    chrome.readingMode.initAxPositionWithNode(textNode.axNodeId);
  }

  moveSpeechForward() {
    chrome.readingMode.movePositionToNextGranularity();
  }

  moveSpeechBackwards() {
    chrome.readingMode.movePositionToPreviousGranularity();
  }
}

// Read aloud model implementation based in TS to be used when the
// ReadAnythingReadAloudTSTextSegmentation flag is enabled.
class TsReadModelImpl implements ReadAloudModelBrowserProxy {
  // TODO: crbug.com/440400392- Implement all of the ReadAloudModelBrowserProxy
  // methods.
  getHighlightForCurrentSegmentIndex():
      Segment[] {
    return [];
  }

  getCurrentTextSegments(): Segment[] {
    return [];
  }

  getCurrentTextContent(): string {
    return '';
  }

  getAccessibleBoundary(text: string, maxSpeechLength: number): number {
    return TextSegmenter.getInstance().getAccessibleBoundary(
        text, maxSpeechLength);
  }

  resetSpeechToBeginning(): void {
    return;
  }

  moveSpeechForward(): void {
    return;
  }

  moveSpeechBackwards() {
    return;
  }

  isInitialized(): boolean {
    return false;
  }

  init(): void {
    return;
  }
}

export function getReadAloudModel(): ReadAloudModelBrowserProxy {
  return instance ||
      (chrome.readingMode.isTsTextSegmentationEnabled ?
           instance = new TsReadModelImpl() :
           instance = new V8ModelImpl());
}

export function setInstance(obj: ReadAloudModelBrowserProxy) {
  instance = obj;
}

let instance: ReadAloudModelBrowserProxy|null = null;
