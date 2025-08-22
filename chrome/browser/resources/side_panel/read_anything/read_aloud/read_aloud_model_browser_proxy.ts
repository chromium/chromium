// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: crbug.com/440400392- Add a TestModelBrowserProxyImpl to replace
// FakeReadingMode.

// Proxy class used to wrap text segmentation calls. This can be used to use
// different text segmentation approaches via feature flag, such as an
// implementation in C++ and an implementation in TypeScript.
export interface ReadAloudModelBrowserProxy {
  // TODO: crbug.com/440400392- Ensure all methods have documentation once
  // the structure is finalized.
  getCurrentText(): number[];
  getHighlightForCurrentSegmentIndex(index: number, phrases: boolean):
      Array<{nodeId: number, start: number, length: number}>;
  getCurrentTextStartIndex(nodeId: number): number;
  getCurrentTextEndIndex(nodeId: number): number;
  getAccessibleBoundary(text: string, maxSpeechLength: number): number;

  // Handle speech positioning.
  resetSpeechToBeginning(): void;
  moveSpeechForward(): void;
  moveSpeechBackwards(): void;

  // TODO: crbug.com/440400392- isSpeechTreeInitialized and onFirstTextNode
  // are tied to the V8 implementation. Investigation how these could be
  // tied into other more generic methods instead.
  isSpeechTreeInitialized(): boolean;
  onFirstTextNode(textNodeId: number): void;
}

class V8ModelImpl implements ReadAloudModelBrowserProxy {

  getCurrentText(): number[] {
    return chrome.readingMode.getCurrentText();
  }

  getHighlightForCurrentSegmentIndex(index: number, phrases: boolean):
      Array<{nodeId: number, start: number, length: number}> {
    return chrome.readingMode.getHighlightForCurrentSegmentIndex(
        index, phrases);
  }

  getCurrentTextStartIndex(nodeId: number): number {
    return chrome.readingMode.getCurrentTextStartIndex(nodeId);
  }

  getCurrentTextEndIndex(nodeId: number): number {
    return chrome.readingMode.getCurrentTextEndIndex(nodeId);
  }

  resetSpeechToBeginning(): void {
    chrome.readingMode.resetGranularityIndex();
  }

  getAccessibleBoundary(text: string, maxSpeechLength: number): number {
    return chrome.readingMode.getAccessibleBoundary(text, maxSpeechLength);
  }

  isSpeechTreeInitialized(): boolean {
    return chrome.readingMode.isSpeechTreeInitialized;
  }

  onFirstTextNode(textNodeId: number) {
    chrome.readingMode.initAxPositionWithNode(textNodeId);
  }

  moveSpeechForward() {
    chrome.readingMode.movePositionToNextGranularity();
  }

  moveSpeechBackwards() {
    chrome.readingMode.movePositionToPreviousGranularity();
  }
}

export function getReadAloudModel(): ReadAloudModelBrowserProxy {
  return instance || (instance = new V8ModelImpl());
}

export function setInstance(obj: ReadAloudModelBrowserProxy) {
  instance = obj;
}

let instance: ReadAloudModelBrowserProxy|null = null;
