// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/447427066): Move this into read_anything.mojom once the
// options are finalized and line focus is stored in prefs.
export enum LineFocusType {
  NONE = 0,
  LINE = 1,
  WINDOW = 2,
}

export interface LineFocus {
  type: LineFocusType;
  lines: number;
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
}
