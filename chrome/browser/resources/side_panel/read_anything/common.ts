// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';

// Determined by experimentation - can be adjusted to fine tune for different
// platforms.
export const minOverflowLengthToScroll = 75;
export const spinnerDebounceTimeout = 150;
export const playFromSelectionTimeout = spinnerDebounceTimeout + 25;
export const toastDurationMs = 10000;

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

const ACTIVE_CSS_CLASS = 'active';

export function getCurrentSpeechRate(): number {
  return parseFloat(chrome.readingMode.speechRate.toFixed(1));
}

// Propagates a custom event with the given name and any details.
export function emitEvent(
    target: EventTarget, name: string, eventDetail?: any) {
  target.dispatchEvent(new CustomEvent(name, {
    bubbles: true,
    composed: true,
    detail: eventDetail,
  }));
}

export function openMenu(
    menuToOpen: CrActionMenuElement, target: HTMLElement,
    showAtConfig?: {minX: number, maxX: number}) {
  // The button should stay active while the menu is open and deactivate when
  // the menu closes.
  menuToOpen.addEventListener('close', () => {
    target.classList.remove(ACTIVE_CSS_CLASS);
  });
  target.classList.add(ACTIVE_CSS_CLASS);

  // TODO(b/337058857): We shouldn't need to wrap this twice in
  // requestAnimationFrame in order to get an accessible label to be read by
  // ChromeVox. We should investigate more in what's going on with
  // cr-action-menu to find a better long-term solution. This is sufficient
  // for now.
  requestAnimationFrame(() => {
    requestAnimationFrame(() => {
      const minY = target.getBoundingClientRect().bottom;
      menuToOpen.showAt(
          target,
          Object.assign(
              {
                minY: minY,
                anchorAlignmentX: AnchorAlignment.AFTER_START,
                anchorAlignmentY: AnchorAlignment.AFTER_END,
                noOffset: true,
              },
              showAtConfig));
    });
  });
}
