// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for accessing and updating visual presentation, layout, and theme
// settings (such as fonts, line and letter spacing, color themes, line focus,
// and presentation states) in Read Anything.
export interface VisualBrowserProxy {
  getInSidePanelPresentationState(): number;
  getInImmersiveOverlayPresentationState(): number;
  getFontName(): string;
  getSupportedFonts(): string[];

  getStandardLineSpacing(): number;
  getLooseLineSpacing(): number;
  getVeryLooseLineSpacing(): number;

  getStandardLetterSpacing(): number;
  getWideLetterSpacing(): number;
  getVeryWideLetterSpacing(): number;

  onFontChange(font: string): void;
  onLineSpacingChange(value: number): void;
  onLetterSpacingChange(value: number): void;
  togglePresentation(): void;
}

export class VisualBrowserProxyImpl implements VisualBrowserProxy {
  getInSidePanelPresentationState(): number {
    return chrome.readingMode.inSidePanelPresentationState;
  }

  getInImmersiveOverlayPresentationState(): number {
    return chrome.readingMode.inImmersiveOverlayPresentationState;
  }

  getFontName(): string {
    return chrome.readingMode.fontName;
  }

  getSupportedFonts(): string[] {
    return chrome.readingMode.supportedFonts;
  }

  getStandardLineSpacing(): number {
    return chrome.readingMode.standardLineSpacing;
  }

  getLooseLineSpacing(): number {
    return chrome.readingMode.looseLineSpacing;
  }

  getVeryLooseLineSpacing(): number {
    return chrome.readingMode.veryLooseLineSpacing;
  }

  getStandardLetterSpacing(): number {
    return chrome.readingMode.standardLetterSpacing;
  }

  getWideLetterSpacing(): number {
    return chrome.readingMode.wideLetterSpacing;
  }

  getVeryWideLetterSpacing(): number {
    return chrome.readingMode.veryWideLetterSpacing;
  }

  onFontChange(font: string): void {
    chrome.readingMode.onFontChange(font);
  }

  onLineSpacingChange(value: number): void {
    chrome.readingMode.onLineSpacingChange(value);
  }

  onLetterSpacingChange(value: number): void {
    chrome.readingMode.onLetterSpacingChange(value);
  }

  togglePresentation(): void {
    chrome.readingMode.togglePresentation();
  }

  static getInstance(): VisualBrowserProxy {
    return instance || (instance = new VisualBrowserProxyImpl());
  }

  static setInstance(obj: VisualBrowserProxy) {
    instance = obj;
  }
}

let instance: VisualBrowserProxy|null = null;
