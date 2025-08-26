// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: crbug.com/440400392- Add a TestModelBrowserProxyImpl to replace
// FakeReadingMode.

// Wrapper class to represent a node used by read aloud. The type of node
// could be either a DOM node or an AXNode depending on what type of text
// segmentation method is used.
export abstract class ReadAloudNode {
  abstract equals(other: ReadAloudNode|undefined|null): boolean;
}

export class AxReadAloudNode extends ReadAloudNode {
  constructor(public readonly axNodeId: number) {
    super();
  }

  equals(other: ReadAloudNode|undefined|null): boolean {
    if (!(other instanceof AxReadAloudNode)) {
      return false;
    }

    return this.axNodeId === other.axNodeId;
  }
}

// Proxy class used to wrap text segmentation calls. This can be used to use
// different text segmentation approaches via feature flag, such as an
// implementation in C++ and an implementation in TypeScript.
export interface ReadAloudModelBrowserProxy {
  // TODO: crbug.com/440400392- Ensure all methods have documentation once
  // the structure is finalized.
  getCurrentText(): ReadAloudNode[];
  getHighlightForCurrentSegmentIndex(index: number, phrases: boolean):
      Array<{node: ReadAloudNode, start: number, length: number}>;
  getCurrentTextStartIndex(node: ReadAloudNode): number;
  getCurrentTextEndIndex(node: ReadAloudNode): number;
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
  getCurrentText(): ReadAloudNode[] {
    return chrome.readingMode.getCurrentText().map(
        id => new AxReadAloudNode(id));
  }

  getHighlightForCurrentSegmentIndex(index: number, phrases: boolean):
      Array<{node: ReadAloudNode, start: number, length: number}> {
    return chrome.readingMode.getHighlightForCurrentSegmentIndex(index, phrases)
        .map(
            ({nodeId, start, length}) =>
                ({node: new AxReadAloudNode(nodeId), start, length}));
  }

  getCurrentTextStartIndex(node: ReadAloudNode): number {
    if (!(node instanceof AxReadAloudNode)) {
      return -1;
    }
    return chrome.readingMode.getCurrentTextStartIndex(node.axNodeId);
  }

  getCurrentTextEndIndex(node: ReadAloudNode): number {
    if (!(node instanceof AxReadAloudNode)) {
      return -1;
    }
    return chrome.readingMode.getCurrentTextEndIndex(node.axNodeId);
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

export function getReadAloudModel(): ReadAloudModelBrowserProxy {
  return instance || (instance = new V8ModelImpl());
}

export function setInstance(obj: ReadAloudModelBrowserProxy) {
  instance = obj;
}

let instance: ReadAloudModelBrowserProxy|null = null;
