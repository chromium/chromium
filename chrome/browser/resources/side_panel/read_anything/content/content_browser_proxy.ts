// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';

import {EventForwarder} from './read_anything_types.js';

export interface AxSegment {
  axNodeId: number;
  start: number;
  end: number;
  axNodeOffset: number;
}

export interface SkiaImageBitmap {
  data: Uint8ClampedArray;
  width: number;
  height: number;
  scale: number;
}

// Browser proxy for document distillation, AXTree node hierarchy navigation,
// selection tracking, DOM anchor mapping, and content rendering callbacks.
export interface ContentBrowserProxy {
  //////////////////////////////////////////////////////////////////////////////
  // Incoming events (C++ -> TypeScript):

  onAnchorsReadyForReadability: ChromeEvent<() => void>;
  onImageDownloaded: ChromeEvent<(nodeId: number) => void>;
  onNodeWillBeDeleted: ChromeEvent<(nodeId: number) => void>;
  onMainFrameSameDocumentNavigation: ChromeEvent<(url: string) => void>;
  onRenderedTextMappingReady: ChromeEvent<() => void>;

  showEmpty: ChromeEvent<() => void>;
  showLoading: ChromeEvent<() => void>;
  updateImages: ChromeEvent<() => void>;
  updateLinks: ChromeEvent<() => void>;
  updateSelection: ChromeEvent<() => void>;
  updateContent: ChromeEvent<() => void>;

  //////////////////////////////////////////////////////////////////////////////
  // Outgoing calls (TypeScript -> C++):

  getStartNodeId(): number;
  getStartOffset(): number;
  getEndNodeId(): number;
  getEndOffset(): number;
  getTextContent(nodeId: number): string;
  getPrefixText(nodeId: number): string;

  getRootId(): number;
  getHtmlTitle(): string;
  getHtmlContent(): string;
  getDocumentUrl(): string;
  getUnexpectedUpdateContentStopSource(): number;
  getAxMapping(index: number): AxSegment[];
  getHtmlTag(nodeId: number): string;
  getUrl(nodeId: number): string;
  getHtmlId(nodeId: number): string;
  getTextDirection(nodeId: number): string;
  getAltText(nodeId: number): string;
  getLanguage(nodeId: number): string;
  getChildren(nodeId: number): number[];
  getImageBitmap(nodeId: number): SkiaImageBitmap|null;
  getAxTreeAnchors(): Record<string, AxTreeAnchorMetadata[]>;

  getActiveDistillationMethod(): number;
  getDistillationTypeReadability(): number;
  getDistillationTypeScreen2x(): number;

  hasValidSelection(): boolean;
  isReadabilityEnabled(): boolean;
  isReadabilitySelectTextEnabled(): boolean;
  isGoogleDocs(): boolean;
  isDocsLoadMoreButtonVisible(): boolean;
  isLeafNode(nodeId: number): boolean;
  isOverline(nodeId: number): boolean;
  shouldBold(nodeId: number): boolean;
  requiresDistillation(): boolean;

  attemptLogEarlySelection(fromSidePanel: boolean): void;
  onNoTextContent(): void;
  onCopy(): void;
  onDistilled(wordCount: number): void;
  onLinkClicked(nodeId: number): void;
  onRenderedTextBlocksAvailable(blocks: string[]): void;
  onConnected(): void;
  onCollapseSelection(): void;
  onSelectionChange(
      anchorNodeId: number, anchorOffset: number, focusNodeId: number,
      focusOffset: number): void;
  onScroll(scrollingOnSelection: boolean): void;
  onScrolledToBottom(): void;
}

export class ContentBrowserProxyImpl implements ContentBrowserProxy {
  onAnchorsReadyForReadability = new EventForwarder<() => void>();
  onImageDownloaded = new EventForwarder<(nodeId: number) => void>();
  onNodeWillBeDeleted = new EventForwarder<(nodeId: number) => void>();
  onMainFrameSameDocumentNavigation =
      new EventForwarder<(url: string) => void>();
  onRenderedTextMappingReady = new EventForwarder<() => void>();
  showEmpty = new EventForwarder<() => void>();
  showLoading = new EventForwarder<() => void>();
  updateImages = new EventForwarder<() => void>();
  updateLinks = new EventForwarder<() => void>();
  updateSelection = new EventForwarder<() => void>();
  updateContent = new EventForwarder<() => void>();

  constructor() {
    chrome.readingMode.onAnchorsReadyForReadability = () => {
      this.onAnchorsReadyForReadability.forward();
    };

    chrome.readingMode.onImageDownloaded = (nodeId: number) => {
      this.onImageDownloaded.forward(nodeId);
    };

    chrome.readingMode.onNodeWillBeDeleted = (nodeId: number) => {
      this.onNodeWillBeDeleted.forward(nodeId);
    };

    chrome.readingMode.onMainFrameSameDocumentNavigation = (url: string) => {
      this.onMainFrameSameDocumentNavigation.forward(url);
    };

    chrome.readingMode.onRenderedTextMappingReady = () => {
      this.onRenderedTextMappingReady.forward();
    };

    chrome.readingMode.showEmpty = () => {
      this.showEmpty.forward();
    };

    chrome.readingMode.showLoading = () => {
      this.showLoading.forward();
    };

    chrome.readingMode.updateImages = () => {
      this.updateImages.forward();
    };

    chrome.readingMode.updateLinks = () => {
      this.updateLinks.forward();
    };

    chrome.readingMode.updateSelection = () => {
      this.updateSelection.forward();
    };

    chrome.readingMode.updateContent = () => {
      this.updateContent.forward();
    };
  }

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

  isReadabilityEnabled(): boolean {
    return chrome.readingMode.isReadabilityEnabled;
  }

  isReadabilitySelectTextEnabled(): boolean {
    return chrome.readingMode.isReadabilitySelectTextEnabled;
  }

  isGoogleDocs(): boolean {
    return chrome.readingMode.isGoogleDocs;
  }

  isLeafNode(nodeId: number): boolean {
    return chrome.readingMode.isLeafNode(nodeId);
  }

  isOverline(nodeId: number): boolean {
    return chrome.readingMode.isOverline(nodeId);
  }

  shouldBold(nodeId: number): boolean {
    return chrome.readingMode.shouldBold(nodeId);
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

  onNoTextContent(): void {
    chrome.readingMode.onNoTextContent();
  }

  onLinkClicked(nodeId: number): void {
    chrome.readingMode.onLinkClicked(nodeId);
  }

  onRenderedTextBlocksAvailable(blocks: string[]): void {
    chrome.readingMode.onRenderedTextBlocksAvailable(blocks);
  }

  getActiveDistillationMethod(): number {
    return chrome.readingMode.activeDistillationMethod;
  }

  getDistillationTypeReadability(): number {
    return chrome.readingMode.distillationTypeReadability;
  }

  getDistillationTypeScreen2x(): number {
    return chrome.readingMode.distillationTypeScreen2x;
  }

  getRootId(): number {
    return chrome.readingMode.rootId;
  }

  getHtmlTitle(): string {
    return chrome.readingMode.htmlTitle;
  }

  getHtmlContent(): string {
    return chrome.readingMode.htmlContent;
  }

  getDocumentUrl(): string {
    return chrome.readingMode.documentUrl;
  }

  getUnexpectedUpdateContentStopSource(): number {
    return chrome.readingMode.unexpectedUpdateContentStopSource;
  }

  getAxMapping(index: number): AxSegment[] {
    return chrome.readingMode.getAxMapping(index);
  }

  getHtmlTag(nodeId: number): string {
    return chrome.readingMode.getHtmlTag(nodeId);
  }

  getUrl(nodeId: number): string {
    return chrome.readingMode.getUrl(nodeId);
  }

  getHtmlId(nodeId: number): string {
    return chrome.readingMode.getHtmlId(nodeId);
  }

  getTextDirection(nodeId: number): string {
    return chrome.readingMode.getTextDirection(nodeId);
  }

  getAltText(nodeId: number): string {
    return chrome.readingMode.getAltText(nodeId);
  }

  getLanguage(nodeId: number): string {
    return chrome.readingMode.getLanguage(nodeId);
  }

  getChildren(nodeId: number): number[] {
    return chrome.readingMode.getChildren(nodeId);
  }

  getImageBitmap(nodeId: number): SkiaImageBitmap|null {
    return chrome.readingMode.getImageBitmap(nodeId);
  }

  getAxTreeAnchors(): Record<string, AxTreeAnchorMetadata[]> {
    return chrome.readingMode.axTreeAnchors;
  }

  isDocsLoadMoreButtonVisible(): boolean {
    return chrome.readingMode.isDocsLoadMoreButtonVisible;
  }

  onScrolledToBottom(): void {
    chrome.readingMode.onScrolledToBottom();
  }

  onCopy(): void {
    chrome.readingMode.onCopy();
  }

  requiresDistillation(): boolean {
    return chrome.readingMode.requiresDistillation;
  }

  onDistilled(wordCount: number): void {
    chrome.readingMode.onDistilled(wordCount);
  }

  static getInstance(): ContentBrowserProxy {
    return instance || (instance = new ContentBrowserProxyImpl());
  }

  static setInstance(obj: ContentBrowserProxy) {
    instance = obj;
  }
}

let instance: ContentBrowserProxy|null = null;
