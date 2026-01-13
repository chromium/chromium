// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';

export enum LineFocusType {
  NONE = 0,
  LINE = 1,
  WINDOW = 2,
}

export enum LineFocusMovement {
  STATIC = 0,
  CURSOR = 1,
}

export class LineFocus {
  static readonly OFF = new LineFocus(
      LineFocusType.NONE, 0, () => chrome.readingMode.lineFocusOff);
  static readonly ONE_LINE_WINDOW = new LineFocus(
      LineFocusType.WINDOW, 1,
      () => chrome.readingMode.lineFocusSmallCursorWindow);
  static readonly THREE_LINE_WINDOW = new LineFocus(
      LineFocusType.WINDOW, 3,
      () => chrome.readingMode.lineFocusMediumCursorWindow);
  static readonly FIVE_LINE_WINDOW = new LineFocus(
      LineFocusType.WINDOW, 5,
      () => chrome.readingMode.lineFocusLargeCursorWindow);
  static readonly STATIC_LINE = new LineFocus(
      LineFocusType.LINE, 1, () => chrome.readingMode.lineFocusStaticLine);
  static readonly CURSOR_LINE = new LineFocus(
      LineFocusType.LINE, 1, () => chrome.readingMode.lineFocusCursorLine);

  private static readonly VALUES = [
    LineFocus.OFF,
    LineFocus.ONE_LINE_WINDOW,
    LineFocus.THREE_LINE_WINDOW,
    LineFocus.FIVE_LINE_WINDOW,
    LineFocus.STATIC_LINE,
    LineFocus.CURSOR_LINE,
  ] as const;

  // Private constructor prevents others from creating new options.
  // enumValue is a function because that value can change in tests after the
  // LineFocus values are already initialized.
  private constructor(
      public readonly type: LineFocusType, public readonly lines: number,
      public readonly enumValue: () => number) {}

  static fromEnumValue(enumValue: number): LineFocus|undefined {
    return this.VALUES.find(value => value.enumValue() === enumValue);
  }

  // TODO(crbug.com/447427066): Finalize the default mode. This is a
  // placeholder.
  static defaultValue(): LineFocus {
    return this.THREE_LINE_WINDOW;
  }
}

export class LineFocusStyle {
  static readonly OFF = new LineFocusStyle(LineFocusType.NONE, 0);
  static readonly SMALL_WINDOW = new LineFocusStyle(LineFocusType.WINDOW, 1);
  static readonly MEDIUM_WINDOW = new LineFocusStyle(LineFocusType.WINDOW, 3);
  static readonly LARGE_WINDOW = new LineFocusStyle(LineFocusType.WINDOW, 5);
  static readonly UNDERLINE = new LineFocusStyle(LineFocusType.LINE, 1);

  private constructor(
      public readonly type: LineFocusType, public readonly lines: number) {}

  // TODO(crbug.com/447427066): Finalize the default mode. This is a
  // placeholder.
  static defaultValue(): LineFocusStyle {
    return this.MEDIUM_WINDOW;
  }

  equals(other: LineFocusStyle): boolean {
    return this.type === other.type && this.lines === other.lines;
  }
}

// Events emitted from the toolbar to the app
export enum ToolbarEvent {
  LETTER_SPACING = 'letter-spacing-change',
  LINE_SPACING = 'line-spacing-change',
  THEME = 'theme-change',
  FONT_SIZE = 'font-size-change',
  FONT = 'font-change',
  RATE = 'rate-change',
  PLAY_PAUSE = 'play-pause-click',
  HIGHLIGHT_CHANGE = 'highlight-change',
  NEXT_GRANULARITY = 'next-granularity-click',
  PREVIOUS_GRANULARITY = 'previous-granularity-click',
  LINKS = 'links-toggle',
  IMAGES = 'images-toggle',
  VOICE = 'select-voice',
  LANGUAGE_TOGGLE = 'voice-language-toggle',
  PLAY_PREVIEW = 'preview-voice',
  LANGUAGE_MENU_OPEN = 'language-menu-open',
  LANGUAGE_MENU_CLOSE = 'language-menu-close',
  VOICE_MENU_OPEN = 'voice-menu-open',
  VOICE_MENU_CLOSE = 'voice-menu-close',
  LINE_FOCUS = 'line-focus-change',
  CLOSE_ALL_MENUS = 'close-all-menus',
  OPEN_SETTINGS_SUBMENU = 'open-settings-submenu',
  PRESENTATION_CHANGE = 'presentation-change',
}

// The available menu items in Reading mode
export enum SettingsOption {
  COLOR = 'color',
  FONT = 'font',
  FONT_SIZE = 'font-size',
  IMAGES = 'images',
  LETTER_SPACING = 'letter-spacing',
  LINE_FOCUS = 'line-focus',
  LINE_SPACING = 'line-spacing',
  LINKS = 'links',
  PRESENTATION = 'presentation',
  VOICE_HIGHLIGHT = 'voice-highlight',
  VOICE_SELECTION = 'voice-selection',
}

// The user settings stored in preferences and restored on re-opening Reading
// mode. Used to set the initial values for the toolbar buttons and menus.
export interface SettingsPrefs {
  letterSpacing: number;
  lineSpacing: number;
  theme: number;
  speechRate: number;
  font: string;
  highlightGranularity: number;
  lineFocus: number;
  linksEnabled: boolean;
  imagesEnabled: boolean;
}
export const DEFAULT_SETTINGS: SettingsPrefs = {
  letterSpacing: 0,
  lineSpacing: 0,
  theme: 0,
  speechRate: 0,
  font: '',
  highlightGranularity: 0,
  lineFocus: 0,
  linksEnabled: false,
  imagesEnabled: false,
};

export interface ShowAtConfigPrefs {
  anchorAlignmentX?: AnchorAlignment;
  anchorAlignmentY?: AnchorAlignment;
  maxX?: number;
  minX?: number;
  minY?: number;
}
