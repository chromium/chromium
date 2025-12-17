// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {LineFocusType} from '../content/read_anything_types.js';

// Constants for styling the app when page zoom changes.
const OVERFLOW_X_TYPICAL = 'hidden';
const OVERFLOW_X_SCROLL = 'scroll';
const MIN_WIDTH_TYPICAL = 'auto';
const MIN_WIDTH_OVERFLOW = 'fit-content';
// Empty state colors.
const EMPTY_STATE_HEADING = 'var(--color-read-anything-foreground';
const EMPTY_STATE_BODY_DEFAULT =
    'var(--color-side-panel-card-secondary-foreground)';
// Container colors.
const BACKGROUND_DEFAULT = 'var(--color-sys-base-container-elevated)';
const BACKGROUND_CUSTOM = 'var(--color-read-anything-background';
const FOREGROUND_DEFAULT = 'var(--color-sys-on-surface)';
const FOREGROUND_CUSTOM = 'var(--color-read-anything-foreground';
const TRANSPARENT = 'transparent';
// User text selection colors.
const SELECTION_BACKGROUND_DEFAULT = 'var(--color-text-selection-background)';
const SELECTION_BACKGROUND_CUSTOM = 'var(--color-read-anything-text-selection';
const SELECTION_FOREGROUND_DEFAULT = 'var(--color-text-selection-foreground)';
const SELECTION_FOREGROUND_DARK = 'var(--google-grey-900)';
const SELECTION_FOREGROUND_LIGHT = 'var(--google-grey-800)';
// Read aloud highlight colors.
const HIGHLIGHT_CURRENT =
    'var(--color-read-anything-current-read-aloud-highlight';
const HIGHLIGHT_PREVIOUS_DEFAULT = 'var(--color-sys-on-surface-subtle)';
const HIGHLIGHT_PREVIOUS_CUSTOM =
    'var(--color-read-anything-previous-read-aloud-highlight';
// Link colors.
const LINK_DEFAULT = 'var(--color-read-anything-link-default';
const LINK_VISITED = 'var(--color-read-anything-link-visited';
// Line focus styles.
// Determined by experimentation to balance visibility without risking
// obstructing any text.
const LINE_FOCUS_LINE_HEIGHT_SCALE = 2;
const LINE_FOCUS_BOX_SHADOW_LINE = 'none';
const LINE_FOCUS_BOX_SHADOW_WINDOW =
    '0 0 0 9999px var(--color-sys-state-scrim)';
const LINE_FOCUS_BG_LINE_DEFAULT = 'var(--color-sys-state-focus-ring)';
const LINE_FOCUS_BG_LINE_CUSTOM = 'var(--color-read-anything-line-focus';
const LINE_FOCUS_BG_WINDOW = 'none';

// Suffixes used in combination with the color vars above to get the color
// values for the current theme.
enum ColorSuffix {
  DEFAULT = '',
  DARK = '-dark',
  LIGHT = '-light',
  YELLOW = '-yellow',
  BLUE = '-blue',
  HIGH_CONTRAST = '-high-contrast',
  LOW_CONTRAST = '-low-contrast',
  SEPIA_LIGHT = '-sepia-light',
  SEPIA_DARK = '-sepia-dark',
}

// Handles updating the visual styles for the Reading mode content panel.
export class AppStyleUpdater {
  private app_: CrLitElement;

  constructor(app: CrLitElement) {
    this.app_ = app;
  }

  setMaxLineWidth() {
    this.setStyle_('--max-width', `${chrome.readingMode.maxLineWidth}ch`);
  }

  setLineFocusPos(y: number, height: number|null, container: HTMLElement) {
    const containerTop = container.offsetTop;
    const containerHeight = container.offsetHeight;
    this.setStyle_('--line-focus-y', `${y}px`);
    this.setStyle_('--line-focus-clip-top', `-${y - containerTop}px`);
    if (height) {
      this.setStyle_('--line-focus-height', `${height}px`);
      this.setStyle_(
          '--line-focus-clip-bottom',
          `${- (containerHeight - y - height + containerTop)}px`);
    }
  }

  setLineFocusStyle(type?: LineFocusType) {
    if (type === undefined || !chrome.readingMode.isLineFocusEnabled ||
        type === LineFocusType.NONE) {
      this.setStyle_('--line-focus-display', 'none');
      return;
    }

    const isWindow = type === LineFocusType.WINDOW;
    this.setLineFocusHeight();
    this.setStyle_(
        '--line-focus-shadow',
        isWindow ? LINE_FOCUS_BOX_SHADOW_WINDOW : LINE_FOCUS_BOX_SHADOW_LINE);
    const lineFocusBgLine =
        this.getLineFocusColor_(this.getCurrentColorSuffix_());
    this.setStyle_(
        '--line-focus-bg', isWindow ? LINE_FOCUS_BG_WINDOW : lineFocusBgLine);
    this.setStyle_('--line-focus-display', 'block');
  }

  setLineFocusHeight() {
    // The height of the line focus underline should be dependent on the font
    // size. This height should be overridden dynamically if the line focus is a
    // window.
    this.setStyle_(
        '--line-focus-height',
        `${chrome.readingMode.fontSize * LINE_FOCUS_LINE_HEIGHT_SCALE}px`);
  }

  setAllTextStyles() {
    this.setLineSpacing();
    this.setLetterSpacing();
    this.setFont();
    this.setFontSize();
    this.setTheme();
  }

  setLineSpacing() {
    this.setStyle_(
        '--line-height',
        `${
            chrome.readingMode.getLineSpacingValue(
                chrome.readingMode.lineSpacing)}`);
  }

  setLetterSpacing() {
    const letterSpacing = chrome.readingMode.getLetterSpacingValue(
        chrome.readingMode.letterSpacing);
    this.setStyle_('--letter-spacing', letterSpacing + 'em');
  }

  setFontSize() {
    this.setStyle_('--font-size', chrome.readingMode.fontSize + 'em');
  }

  setFont() {
    this.setStyle_(
        '--font-family',
        chrome.readingMode.getValidatedFontName(chrome.readingMode.fontName));
  }

  setHighlight() {
    this.setStyle_(
        '--current-highlight-bg-color',
        this.getCurrentHighlightColor_(this.getCurrentColorSuffix_()));
  }

  resetToolbar() {
    this.setStyle_('--app-overflow-x', OVERFLOW_X_TYPICAL);
    this.setStyle_('--container-min-width', MIN_WIDTH_TYPICAL);
  }

  overflowToolbar(shouldScroll: boolean) {
    this.setStyle_(
        '--app-overflow-x',
        shouldScroll ? OVERFLOW_X_SCROLL : OVERFLOW_X_TYPICAL);
    this.setStyle_(
        // When we scroll, we should allow the container to expand and scroll
        // horizontally.
        '--container-min-width',
        shouldScroll ? MIN_WIDTH_OVERFLOW : MIN_WIDTH_TYPICAL);
  }

  setTheme() {
    const colorSuffix = this.getCurrentColorSuffix_();
    this.setStyle_('--background-color', this.getBackgroundColor_(colorSuffix));
    this.setStyle_('--foreground-color', this.getForegroundColor_(colorSuffix));
    this.setStyle_('--selection-color', this.getSelectionColor_(colorSuffix));
    this.setStyle_(
        '--current-highlight-bg-color',
        this.getCurrentHighlightColor_(colorSuffix));
    this.setStyle_(
        '--previous-highlight-color',
        this.getPreviousHighlightColor_(colorSuffix));
    this.setStyle_(
        '--sp-empty-state-heading-color',
        `${EMPTY_STATE_HEADING}${colorSuffix})`);
    this.setStyle_(
        '--sp-empty-state-body-color',
        this.getEmptyStateBodyColor_(colorSuffix));
    this.setStyle_('--link-color', `${LINK_DEFAULT}${colorSuffix})`);
    this.setStyle_('--visited-link-color', `${LINK_VISITED}${colorSuffix})`);
    this.setStyle_('--line-focus-bg', this.getLineFocusColor_(colorSuffix));

    document.documentElement.style.setProperty(
        '--selection-color', this.getSelectionColor_(colorSuffix));
    document.documentElement.style.setProperty(
        '--selection-text-color', this.getSelectionTextColor_(colorSuffix));
  }

  private setStyle_(key: string, val: string) {
    this.app_.style.setProperty(key, val);
  }

  private getCurrentColorSuffix_(): ColorSuffix {
    switch (chrome.readingMode.colorTheme) {
      case chrome.readingMode.lightTheme:
        return ColorSuffix.LIGHT;
      case chrome.readingMode.darkTheme:
        return ColorSuffix.DARK;
      case chrome.readingMode.yellowTheme:
        return ColorSuffix.YELLOW;
      case chrome.readingMode.blueTheme:
        return ColorSuffix.BLUE;
      case chrome.readingMode.highContrastTheme:
        return ColorSuffix.HIGH_CONTRAST;
      case chrome.readingMode.lowContrastTheme:
        return ColorSuffix.LOW_CONTRAST;
      case chrome.readingMode.sepiaLightTheme:
        return ColorSuffix.SEPIA_LIGHT;
      case chrome.readingMode.sepiaDarkTheme:
        return ColorSuffix.SEPIA_DARK;
      default:
        return ColorSuffix.DEFAULT;
    }
  }

  private getEmptyStateBodyColor_(colorSuffix: ColorSuffix): string {
    switch (colorSuffix) {
      case ColorSuffix.DEFAULT:
        return EMPTY_STATE_BODY_DEFAULT;
      default:
        return EMPTY_STATE_HEADING + `${colorSuffix})`;
    }
  }

  private getCurrentHighlightColor_(colorSuffix: ColorSuffix): string {
    if (!chrome.readingMode.isHighlightOn()) {
      return TRANSPARENT;
    }
    if (colorSuffix === ColorSuffix.DEFAULT) {
      return SELECTION_BACKGROUND_DEFAULT;
    }
    return HIGHLIGHT_CURRENT + `${colorSuffix})`;
  }

  private getPreviousHighlightColor_(colorSuffix: ColorSuffix): string {
    return (colorSuffix === ColorSuffix.DEFAULT) ?
        HIGHLIGHT_PREVIOUS_DEFAULT :
        (HIGHLIGHT_PREVIOUS_CUSTOM + `${colorSuffix})`);
  }

  private getBackgroundColor_(colorSuffix: ColorSuffix): string {
    return (colorSuffix === ColorSuffix.DEFAULT) ?
        BACKGROUND_DEFAULT :
        (BACKGROUND_CUSTOM + `${colorSuffix})`);
  }

  private getForegroundColor_(colorSuffix: ColorSuffix): string {
    return (colorSuffix === ColorSuffix.DEFAULT) ?
        FOREGROUND_DEFAULT :
        (FOREGROUND_CUSTOM + `${colorSuffix})`);
  }

  private getSelectionColor_(colorSuffix: ColorSuffix): string {
    return (colorSuffix === ColorSuffix.DEFAULT) ?
        SELECTION_BACKGROUND_DEFAULT :
        (SELECTION_BACKGROUND_CUSTOM + `${colorSuffix})`);
  }

  private getSelectionTextColor_(colorSuffix: ColorSuffix): string {
    if (colorSuffix === ColorSuffix.DEFAULT) {
      return SELECTION_FOREGROUND_DEFAULT;
    }

    return (window.matchMedia('(prefers-color-scheme: dark)').matches) ?
        SELECTION_FOREGROUND_DARK :
        SELECTION_FOREGROUND_LIGHT;
  }

  private getLineFocusColor_(colorSuffix: ColorSuffix): string {
    return (colorSuffix === ColorSuffix.DEFAULT) ?
        LINE_FOCUS_BG_LINE_DEFAULT :
        (LINE_FOCUS_BG_LINE_CUSTOM + `${colorSuffix})`);
  }
}
