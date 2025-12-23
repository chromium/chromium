// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';

export enum LineFocusType {
  NONE = 0,
  LINE = 1,
  WINDOW = 2,
}

export class LineFocus {
  static readonly OFF = new LineFocus(LineFocusType.NONE, 0);
  static readonly ONE_LINE_WINDOW = new LineFocus(LineFocusType.WINDOW, 1);
  static readonly THREE_LINE_WINDOW = new LineFocus(LineFocusType.WINDOW, 3);
  static readonly FIVE_LINE_WINDOW = new LineFocus(LineFocusType.WINDOW, 5);
  static readonly STATIC_LINE = new LineFocus(LineFocusType.LINE, 1, true);
  static readonly CURSOR_LINE = new LineFocus(LineFocusType.LINE, 1);

  // Private constructor prevents others from creating new options
  private constructor(
      public readonly type: LineFocusType, public readonly lines: number,
      public readonly isStatic: boolean = false) {}
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
}

export interface ShowAtConfigPrefs {
  anchorAlignmentX?: AnchorAlignment;
  anchorAlignmentY?: AnchorAlignment;
  maxX?: number;
  minX?: number;
  minY?: number;
}
