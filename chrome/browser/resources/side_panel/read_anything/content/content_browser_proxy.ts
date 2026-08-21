// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Browser proxy for document distillation, AXTree node hierarchy navigation,
// selection tracking, DOM anchor mapping, and content rendering callbacks.
export interface ContentBrowserProxy {
  getStartNodeId(): number;
  getStartOffset(): number;
  getEndNodeId(): number;
  getEndOffset(): number;
  getTextContent(nodeId: number): string;
  getPrefixText(nodeId: number): string;

  hasValidSelection(): boolean;
  isReadabilitySelectTextEnabled(): boolean;

  attemptLogEarlySelection(fromSidePanel: boolean): void;
  onConnected(): void;
  onCollapseSelection(): void;
  onSelectionChange(
      anchorNodeId: number, anchorOffset: number, focusNodeId: number,
      focusOffset: number): void;
  onScroll(scrollingOnSelection: boolean): void;
}

export class ContentBrowserProxyImpl implements ContentBrowserProxy {
  getStartNodeId(): number {
    return chrome.readingMode.startNodeId;
  }

  getStartOffset(): number {
    return chrome.readingMode.startOffset;
  }

  getEndNodeId(): number {
    return chrome.readingMode.endNodeId;
  }

  getEndOffset(): number {
    return chrome.readingMode.endOffset;
  }

  hasValidSelection(): boolean {
    return chrome.readingMode.hasValidSelection;
  }

  isReadabilitySelectTextEnabled(): boolean {
    return chrome.readingMode.isReadabilitySelectTextEnabled;
  }

  onConnected(): void {
    chrome.readingMode.onConnected();
  }

  onCollapseSelection(): void {
    chrome.readingMode.onCollapseSelection();
  }

  attemptLogEarlySelection(fromSidePanel: boolean): void {
    chrome.readingMode.attemptLogEarlySelection(fromSidePanel);
  }

  onSelectionChange(
      anchorNodeId: number, anchorOffset: number, focusNodeId: number,
      focusOffset: number): void {
    chrome.readingMode.onSelectionChange(
        anchorNodeId, anchorOffset, focusNodeId, focusOffset);
  }

  onScroll(scrollingOnSelection: boolean): void {
    chrome.readingMode.onScroll(scrollingOnSelection);
  }

  getTextContent(nodeId: number): string {
    return chrome.readingMode.getTextContent(nodeId);
  }

  getPrefixText(nodeId: number): string {
    return chrome.readingMode.getPrefixText(nodeId);
  }

  static getInstance(): ContentBrowserProxy {
    return instance || (instance = new ContentBrowserProxyImpl());
  }

  static setInstance(obj: ContentBrowserProxy) {
    instance = obj;
  }
}

let instance: ContentBrowserProxy|null = null;
