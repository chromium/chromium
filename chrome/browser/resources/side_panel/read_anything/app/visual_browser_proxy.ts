// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for accessing and updating visual presentation, layout, and theme
// settings (such as fonts, line and letter spacing, color themes, line focus,
// and presentation states) in Read Anything.
export interface VisualBrowserProxy {
  getInSidePanelPresentationState(): number;
  getInImmersiveOverlayPresentationState(): number;
  getActivePresentationState(): number;
  isImmersiveEnabled(): boolean;
  isPdf(): boolean;

  getFontName(): string;
  getSupportedFonts(): string[];

  getStandardLineSpacing(): number;
  getLooseLineSpacing(): number;
  getVeryLooseLineSpacing(): number;
  getLineSpacing(): number;
  getLineSpacingValue(lineSpacing: number): number;
  getFontSize(): number;

  getStandardLetterSpacing(): number;
  getWideLetterSpacing(): number;
  getVeryWideLetterSpacing(): number;

  getDefaultTheme(): number;
  getLightTheme(): number;
  getDarkTheme(): number;
  getYellowTheme(): number;
  getBlueTheme(): number;
  getHighContrastTheme(): number;
  getLowContrastLightTheme(): number;
  getLowContrastDarkTheme(): number;

  getActiveDistillationMethod(): number;
  getDistillationTypeReadability(): number;

  maybeHasKeyPointsSection(): boolean;
  getKeyPointsRegex(): string;

  requestImageData(nodeId: number): void;

  isLineFocusEnabled(): boolean;
  getLineFocusOff(): number;
  getLineFocusSmallStaticWindow(): number;
  getLineFocusMediumStaticWindow(): number;
  getLineFocusLargeStaticWindow(): number;
  getLineFocusSmallCursorWindow(): number;
  getLineFocusMediumCursorWindow(): number;
  getLineFocusLargeCursorWindow(): number;
  getLineFocusStaticLine(): number;
  getLineFocusCursorLine(): number;

  onFontChange(font: string): void;
  onLineSpacingChange(value: number): void;
  onLetterSpacingChange(value: number): void;
  onThemeChange(theme: number): void;
  onLineFocusChanged(value: number, lastNonDisabledValue: number): void;
  togglePresentation(): void;
}

export class VisualBrowserProxyImpl implements VisualBrowserProxy {
  getInSidePanelPresentationState(): number {
    return chrome.readingMode.inSidePanelPresentationState;
  }

  getInImmersiveOverlayPresentationState(): number {
    return chrome.readingMode.inImmersiveOverlayPresentationState;
  }

  getActivePresentationState(): number {
    return chrome.readingMode.activePresentationState;
  }

  isImmersiveEnabled(): boolean {
    return chrome.readingMode.isImmersiveEnabled;
  }

  isPdf(): boolean {
    return chrome.readingMode.isPdf;
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

  getLineSpacing(): number {
    return chrome.readingMode.lineSpacing;
  }

  getLineSpacingValue(lineSpacing: number): number {
    return chrome.readingMode.getLineSpacingValue(lineSpacing);
  }

  getFontSize(): number {
    return chrome.readingMode.fontSize;
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

  getDefaultTheme(): number {
    return chrome.readingMode.defaultTheme;
  }

  getLightTheme(): number {
    return chrome.readingMode.lightTheme;
  }

  getDarkTheme(): number {
    return chrome.readingMode.darkTheme;
  }

  getYellowTheme(): number {
    return chrome.readingMode.yellowTheme;
  }

  getBlueTheme(): number {
    return chrome.readingMode.blueTheme;
  }

  getHighContrastTheme(): number {
    return chrome.readingMode.highContrastTheme;
  }

  getLowContrastLightTheme(): number {
    return chrome.readingMode.lowContrastLightTheme;
  }

  getLowContrastDarkTheme(): number {
    return chrome.readingMode.lowContrastDarkTheme;
  }

  getActiveDistillationMethod(): number {
    return chrome.readingMode.activeDistillationMethod;
  }

  getDistillationTypeReadability(): number {
    return chrome.readingMode.distillationTypeReadability;
  }

  maybeHasKeyPointsSection(): boolean {
    return chrome.readingMode.maybeHasKeyPointsSection();
  }

  getKeyPointsRegex(): string {
    return chrome.readingMode.getKeyPointsRegex();
  }

  requestImageData(nodeId: number): void {
    chrome.readingMode.requestImageData(nodeId);
  }

  isLineFocusEnabled(): boolean {
    return chrome.readingMode.isLineFocusEnabled;
  }

  getLineFocusOff(): number {
    return chrome.readingMode.lineFocusOff;
  }

  getLineFocusSmallStaticWindow(): number {
    return chrome.readingMode.lineFocusSmallStaticWindow;
  }

  getLineFocusMediumStaticWindow(): number {
    return chrome.readingMode.lineFocusMediumStaticWindow;
  }

  getLineFocusLargeStaticWindow(): number {
    return chrome.readingMode.lineFocusLargeStaticWindow;
  }

  getLineFocusSmallCursorWindow(): number {
    return chrome.readingMode.lineFocusSmallCursorWindow;
  }

  getLineFocusMediumCursorWindow(): number {
    return chrome.readingMode.lineFocusMediumCursorWindow;
  }

  getLineFocusLargeCursorWindow(): number {
    return chrome.readingMode.lineFocusLargeCursorWindow;
  }

  getLineFocusStaticLine(): number {
    return chrome.readingMode.lineFocusStaticLine;
  }

  getLineFocusCursorLine(): number {
    return chrome.readingMode.lineFocusCursorLine;
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

  onThemeChange(theme: number): void {
    chrome.readingMode.onThemeChange(theme);
  }

  onLineFocusChanged(value: number, lastNonDisabledValue: number): void {
    chrome.readingMode.onLineFocusChanged(value, lastNonDisabledValue);
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
