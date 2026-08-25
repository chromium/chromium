// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';

import {EventForwarder} from '../content/read_anything_types.js';

// Interface for accessing and updating visual presentation, layout, and theme
// settings (such as fonts, line and letter spacing, color themes, line focus,
// and presentation states) in Read Anything.
export interface VisualBrowserProxy {
  onPinStateReceived: ChromeEvent<(pinState: boolean) => void>;
  onPresentationStateReceived: ChromeEvent<(presentationState: number) => void>;

  getInSidePanelPresentationState(): number;
  getInImmersiveOverlayPresentationState(): number;
  getInHiddenPresentationState(): number;
  getActivePresentationState(): number;
  isImmersiveEnabled(): boolean;
  isReadAnythingImprovedUiEnabled(): boolean;
  isReadAnythingReadAloudExperimentalPlaybackUiEnabled(): boolean;
  isReadAnythingTranslateEntryPointEnabled(): boolean;
  isImagesEnabled(): boolean;
  isLinksEnabled(): boolean;
  isPdf(): boolean;
  shouldShowUi(): boolean;

  getMaxLineWidth(): number;

  getFontName(): string;
  getSupportedFonts(): string[];
  getAllFonts(): string[];
  getValidatedFontName(font: string): string;

  getStandardLineSpacing(): number;
  getLooseLineSpacing(): number;
  getVeryLooseLineSpacing(): number;
  getLineSpacing(): number;
  getLineSpacingValue(lineSpacing: number): number;
  getDefaultFontSize(): number;
  getFontSize(): number;

  getStandardLetterSpacing(): number;
  getWideLetterSpacing(): number;
  getVeryWideLetterSpacing(): number;
  getLetterSpacing(): number;
  getLetterSpacingValue(letterSpacing: number): number;

  getDefaultTheme(): number;
  getLightTheme(): number;
  getDarkTheme(): number;
  getYellowTheme(): number;
  getBlueTheme(): number;
  getHighContrastTheme(): number;
  getLowContrastLightTheme(): number;
  getLowContrastDarkTheme(): number;
  getColorTheme(): number;

  maybeHasKeyPointsSection(): boolean;
  getKeyPointsRegex(): string;

  requestImageData(nodeId: number): void;

  isLineFocusEnabled(): boolean;
  isLineFocusOn(): boolean;
  getLastNonDisabledLineFocus(): number;
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
  onLinksEnabledToggled(): void;
  onImagesEnabledToggled(): void;
  onFontSizeChanged(increase: boolean): void;
  onFontSizeReset(): void;
  onTranslationRequested(): void;

  togglePresentation(): void;
  togglePinState(): void;
  sendPinStateRequest(): void;
  sendGetPresentationStateRequest(): void;
  close(): void;
}

export class VisualBrowserProxyImpl implements VisualBrowserProxy {
  onPinStateReceived = new EventForwarder<(pinState: boolean) => void>();
  onPresentationStateReceived =
      new EventForwarder<(presentationState: number) => void>();

  constructor() {
    chrome.readingMode.onPinStateReceived = (pinState: boolean) => {
      this.onPinStateReceived.forward(pinState);
    };

    chrome.readingMode.onPresentationStateReceived =
        (presentationState: number) => {
          this.onPresentationStateReceived.forward(presentationState);
        };
  }

  getInSidePanelPresentationState(): number {
    return chrome.readingMode.inSidePanelPresentationState;
  }

  getInImmersiveOverlayPresentationState(): number {
    return chrome.readingMode.inImmersiveOverlayPresentationState;
  }

  getInHiddenPresentationState(): number {
    return chrome.readingMode.inHiddenPresentationState;
  }

  getActivePresentationState(): number {
    return chrome.readingMode.activePresentationState;
  }

  isImmersiveEnabled(): boolean {
    return chrome.readingMode.isImmersiveEnabled;
  }

  isReadAnythingReadAloudExperimentalPlaybackUiEnabled(): boolean {
    return chrome.readingMode
        .isReadAnythingReadAloudExperimentalPlaybackUiEnabled;
  }

  isImagesEnabled(): boolean {
    return chrome.readingMode.imagesEnabled;
  }

  isLinksEnabled(): boolean {
    return chrome.readingMode.linksEnabled;
  }

  isPdf(): boolean {
    return chrome.readingMode.isPdf;
  }

  getMaxLineWidth(): number {
    return chrome.readingMode.maxLineWidth;
  }

  getFontName(): string {
    return chrome.readingMode.fontName;
  }

  getSupportedFonts(): string[] {
    return chrome.readingMode.supportedFonts;
  }

  getAllFonts(): string[] {
    return chrome.readingMode.allFonts;
  }

  getValidatedFontName(font: string): string {
    return chrome.readingMode.getValidatedFontName(font);
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

  getDefaultFontSize(): number {
    return 2.0;
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

  getLetterSpacing(): number {
    return chrome.readingMode.letterSpacing;
  }

  getLetterSpacingValue(letterSpacing: number): number {
    return chrome.readingMode.getLetterSpacingValue(letterSpacing);
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

  getColorTheme(): number {
    return chrome.readingMode.colorTheme;
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

  onFontSizeChanged(increase: boolean): void {
    chrome.readingMode.onFontSizeChanged(increase);
  }

  onFontSizeReset(): void {
    chrome.readingMode.onFontSizeReset();
  }

  isReadAnythingTranslateEntryPointEnabled(): boolean {
    return chrome.readingMode.isReadAnythingTranslateEntryPointEnabled;
  }

  onTranslationRequested(): void {
    chrome.readingMode.onTranslationRequested();
  }

  togglePresentation(): void {
    chrome.readingMode.togglePresentation();
  }

  close(): void {
    chrome.readingMode.close();
  }

  onLinksEnabledToggled(): void {
    chrome.readingMode.onLinksEnabledToggled();
  }

  onImagesEnabledToggled(): void {
    chrome.readingMode.onImagesEnabledToggled();
  }

  isReadAnythingImprovedUiEnabled(): boolean {
    return chrome.readingMode.isReadAnythingImprovedUiEnabled;
  }

  togglePinState(): void {
    chrome.readingMode.togglePinState();
  }

  sendPinStateRequest(): void {
    chrome.readingMode.sendPinStateRequest();
  }

  sendGetPresentationStateRequest(): void {
    chrome.readingMode.sendGetPresentationStateRequest();
  }

  shouldShowUi(): boolean {
    return chrome.readingMode.shouldShowUi();
  }

  getLastNonDisabledLineFocus(): number {
    return chrome.readingMode.lastNonDisabledLineFocus;
  }

  isLineFocusOn(): boolean {
    return chrome.readingMode.isLineFocusOn;
  }

  static getInstance(): VisualBrowserProxy {
    return instance || (instance = new VisualBrowserProxyImpl());
  }

  static setInstance(obj: VisualBrowserProxy) {
    instance = obj;
  }
}

let instance: VisualBrowserProxy|null = null;
