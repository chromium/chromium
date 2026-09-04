// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';

import {VisualBrowserProxyImpl} from '../app/visual_browser_proxy.js';

// Helper that implements ChromeEvent to manage and dispatch events from C++
// backend callbacks (e.g. chrome.readingMode) to registered TypeScript
// listeners.
export class EventForwarder<T extends Function> implements ChromeEvent<T> {
  private listeners_: T[] = [];

  addListener(listener: T) {
    this.listeners_.push(listener);
  }

  removeListener(listener: T) {
    this.listeners_ = this.listeners_.filter(l => l !== listener);
  }

  forward(...args: unknown[]) {
    this.listeners_.forEach(l => l(...args));
  }
}

export enum ContentPositionSource {
  SELECTION = 0,
  LINE_FOCUS = 1,
}

export interface ContentPosition {
  node: Node;
  offset: number;
  source: ContentPositionSource;
}

export enum LineFocusType {
  NONE = 0,
  LINE = 1,
  WINDOW = 2,
}

export enum LineFocusMovement {
  STATIC = 0,
  CURSOR = 1,
}

// Used to notify of the type of line focus movement that occurred.
// Some movements should trigger a visual update only, while others should
// also trigger a content update, and others should trigger no visual or
// content update.
export enum LineFocusNotificationType {
  NONE = 0,
  VISUAL = 1,
  CONTENT = 2,
}

export class LineFocusStyle {
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

export const getLineFocusValues = (): Record<number, LineFocusValue> => {
  const visualBrowserProxy = VisualBrowserProxyImpl.getInstance();
  return {
    [visualBrowserProxy.getLineFocusSmallCursorWindow()]: {
      value: visualBrowserProxy.getLineFocusSmallCursorWindow(),
      style: LineFocusStyle.SMALL_WINDOW,
      movement: LineFocusMovement.CURSOR,
    },
    [visualBrowserProxy.getLineFocusSmallStaticWindow()]: {
      value: visualBrowserProxy.getLineFocusSmallStaticWindow(),
      style: LineFocusStyle.SMALL_WINDOW,
      movement: LineFocusMovement.STATIC,
    },
    [visualBrowserProxy.getLineFocusMediumCursorWindow()]: {
      value: visualBrowserProxy.getLineFocusMediumCursorWindow(),
      style: LineFocusStyle.MEDIUM_WINDOW,
      movement: LineFocusMovement.CURSOR,
    },
    [visualBrowserProxy.getLineFocusMediumStaticWindow()]: {
      value: visualBrowserProxy.getLineFocusMediumStaticWindow(),
      style: LineFocusStyle.MEDIUM_WINDOW,
      movement: LineFocusMovement.STATIC,
    },
    [visualBrowserProxy.getLineFocusLargeCursorWindow()]: {
      value: visualBrowserProxy.getLineFocusLargeCursorWindow(),
      style: LineFocusStyle.LARGE_WINDOW,
      movement: LineFocusMovement.CURSOR,
    },
    [visualBrowserProxy.getLineFocusLargeStaticWindow()]: {
      value: visualBrowserProxy.getLineFocusLargeStaticWindow(),
      style: LineFocusStyle.LARGE_WINDOW,
      movement: LineFocusMovement.STATIC,
    },
    [visualBrowserProxy.getLineFocusCursorLine()]: {
      value: visualBrowserProxy.getLineFocusCursorLine(),
      style: LineFocusStyle.UNDERLINE,
      movement: LineFocusMovement.CURSOR,
    },
    [visualBrowserProxy.getLineFocusStaticLine()]: {
      value: visualBrowserProxy.getLineFocusStaticLine(),
      style: LineFocusStyle.UNDERLINE,
      movement: LineFocusMovement.STATIC,
    },
  };
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
  LANGUAGE_SELECTED = 'voice-language-selected',
  PLAY_PREVIEW = 'preview-voice',
  LANGUAGE_MENU_OPEN = 'language-menu-open',
  LANGUAGE_MENU_CLOSE = 'language-menu-close',
  VOICE_MENU_OPEN = 'voice-menu-open',
  VOICE_MENU_CLOSE = 'voice-menu-close',
  LINE_FOCUS_STYLE = 'line-focus-style-change',
  LINE_FOCUS_MOVEMENT = 'line-focus-movement-change',
  LINE_FOCUS_TOGGLE = 'line-focus-toggle-change',
  CLOSE_ALL_MENUS = 'close-all-menus',
  OPEN_SETTINGS_SUBMENU = 'open-settings-submenu',
  PRESENTATION_CHANGE = 'presentation-change',
  CLOSE_SUBMENU_REQUESTED = 'close-submenu-requested',
  SETTINGS_OPENED = 'settings-opened',
  SETTINGS_CLOSED = 'settings-closed',
  TRANSLATION_REQUESTED = 'translation-requested',
}

// The available menu items in Reading mode
export enum SettingsOption {
  APPEARANCE = 'appearance',
  AUDIO = 'audio',
  COLOR = 'color',
  FONT = 'font',
  TEXT = 'text',
  FONT_SIZE = 'font-size',
  IMAGES = 'images',
  LETTER_SPACING = 'letter-spacing',
  LINE_FOCUS = 'line-focus',
  LINE_SPACING = 'line-spacing',
  LINKS = 'links',
  MEDIA = 'media',
  PINNED_TO_TOOLBAR = 'pinned-to-toolbar',
  PRESENTATION = 'presentation',
  TRANSLATION_REQUESTED = 'translation-requested',
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
