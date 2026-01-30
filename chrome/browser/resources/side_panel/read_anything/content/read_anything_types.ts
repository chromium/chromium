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

interface LineFocusValue {
  value: number;
  style: LineFocusStyle;
  movement: LineFocusMovement;
}

let lineFocusValues: Record<number, LineFocusValue>;
export const getLineFocusValues = (): Record<number, LineFocusValue> => {
  if (!lineFocusValues || !lineFocusValues[chrome.readingMode.lineFocusOff]) {
    lineFocusValues = {
      [chrome.readingMode.lineFocusOff]: {
        value: chrome.readingMode.lineFocusOff,
        style: LineFocusStyle.OFF,
        movement: LineFocusMovement.STATIC,
      },
      [chrome.readingMode.lineFocusSmallCursorWindow]: {
        value: chrome.readingMode.lineFocusSmallCursorWindow,
        style: LineFocusStyle.SMALL_WINDOW,
        movement: LineFocusMovement.CURSOR,
      },
      [chrome.readingMode.lineFocusSmallStaticWindow]: {
        value: chrome.readingMode.lineFocusSmallStaticWindow,
        style: LineFocusStyle.SMALL_WINDOW,
        movement: LineFocusMovement.STATIC,
      },
      [chrome.readingMode.lineFocusMediumCursorWindow]: {
        value: chrome.readingMode.lineFocusMediumCursorWindow,
        style: LineFocusStyle.MEDIUM_WINDOW,
        movement: LineFocusMovement.CURSOR,
      },
      [chrome.readingMode.lineFocusMediumStaticWindow]: {
        value: chrome.readingMode.lineFocusMediumStaticWindow,
        style: LineFocusStyle.MEDIUM_WINDOW,
        movement: LineFocusMovement.STATIC,
      },
      [chrome.readingMode.lineFocusLargeCursorWindow]: {
        value: chrome.readingMode.lineFocusLargeCursorWindow,
        style: LineFocusStyle.LARGE_WINDOW,
        movement: LineFocusMovement.CURSOR,
      },
      [chrome.readingMode.lineFocusLargeStaticWindow]: {
        value: chrome.readingMode.lineFocusLargeStaticWindow,
        style: LineFocusStyle.LARGE_WINDOW,
        movement: LineFocusMovement.STATIC,
      },
      [chrome.readingMode.lineFocusCursorLine]: {
        value: chrome.readingMode.lineFocusCursorLine,
        style: LineFocusStyle.UNDERLINE,
        movement: LineFocusMovement.CURSOR,
      },
      [chrome.readingMode.lineFocusStaticLine]: {
        value: chrome.readingMode.lineFocusStaticLine,
        style: LineFocusStyle.UNDERLINE,
        movement: LineFocusMovement.STATIC,
      },
    };
  }
  return lineFocusValues;
};

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
  LINE_FOCUS_STYLE = 'line-focus-style-change',
  LINE_FOCUS_MOVEMENT = 'line-focus-movement-change',
  CLOSE_ALL_MENUS = 'close-all-menus',
  OPEN_SETTINGS_SUBMENU = 'open-settings-submenu',
  PRESENTATION_CHANGE = 'presentation-change',
  CLOSE_SUBMENU_REQUESTED = 'close-submenu-requested',
  SETTINGS_OPENED = 'settings-opened',
  SETTINGS_CLOSED = 'settings-closed',
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
  PINNED_TO_TOOLBAR = 'pinned-to-toolbar',
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
