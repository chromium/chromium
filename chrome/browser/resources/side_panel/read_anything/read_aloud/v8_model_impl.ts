// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ReadAloudModelBrowserProxy} from './read_aloud_model_browser_proxy.js';
import {AxReadAloudNode, ReadAloudNode} from './read_aloud_types.js';
import type {Segment} from './read_aloud_types.js';

export class V8ModelImpl implements ReadAloudModelBrowserProxy {
  getHighlightForCurrentSegmentIndex(index: number, phrases: boolean):
      Segment[] {
    // Since there's a chance createFromAxNode can return an undefined node,
    // filter out the undefined ones.
    return chrome.readingMode.getHighlightForCurrentSegmentIndex(index, phrases)
        .map(
            ({nodeId, start, length}) =>
                ({node: ReadAloudNode.createFromAxNode(nodeId), start, length}))
        .filter((segment): segment is Segment => segment.node !== undefined);
  }

  getCurrentTextSegments(): Segment[] {
    // Since there's a chance createFromAxNode can return an undefined node,
    // filter out the undefined ones.
    return chrome.readingMode.getCurrentTextSegments()
        .map(
            ({nodeId, start, length}) =>
                ({node: ReadAloudNode.createFromAxNode(nodeId), start, length}))
        .filter((segment): segment is Segment => segment.node !== undefined);
  }

  getCurrentTextContent(): string {
    return chrome.readingMode.getCurrentTextContent();
  }

  resetSpeechToBeginning(): void {
    chrome.readingMode.resetGranularityIndex();
  }

  getAccessibleText(text: string, maxSpeechLength: number): string {
    const boundary =
        chrome.readingMode.getAccessibleBoundary(text, maxSpeechLength);
    return text.substring(0, boundary);
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
