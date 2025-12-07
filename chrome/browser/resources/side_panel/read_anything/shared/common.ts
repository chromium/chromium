// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {TextSegmenter} from '../read_aloud/text_segmenter.js';

// Determined by experimentation - can be adjusted to fine tune for different
// platforms.
export const minOverflowLengthToScroll = 75;
export const spinnerDebounceTimeout = 150;
export const playFromSelectionTimeout = spinnerDebounceTimeout + 25;

// How long to delay before logging the empty state. If it's only shown briefly,
// no need to log.
export const LOG_EMPTY_DELAY_MS = 500;

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
// The percent of a view that must be visible to be considered "mostly visible"
// for the purpose of determining what's likely being actually read in the
// reading mode panel.
export const MOSTLY_VISIBLE_PERCENT = 0.8;

export function openMenu(
    menuToOpen: CrActionMenuElement, target: HTMLElement,
    showAtConfig?: {minX: number, maxX: number}, onShow?: () => void) {
  // The button should stay active while the menu is open and deactivate when
  // the menu closes.
  menuToOpen.addEventListener('close', () => {
    target.classList.remove(ACTIVE_CSS_CLASS);
  });
  target.classList.add(ACTIVE_CSS_CLASS);

  // TODO: crbug.com/337058857 - We shouldn't need to wrap this twice in
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
      if (onShow) {
        onShow();
      }
    });
  });
}

// Estimate the word count of the given text using the TextSegmenter class.
export function getWordCount(text: string): number {
  return TextSegmenter.getInstance().getWordCount(text);
}

// Returns true if the given rect is mostly within the visible window.
export function isRectMostlyVisible(rect: DOMRect): boolean {
  if (rect.height <= 0) {
    return false;
  }
  const isTopMostlyVisible = isPointVisible(rect.top) &&
      isPointVisible(rect.top + (rect.height * MOSTLY_VISIBLE_PERCENT));
  const isBottomMostlyVisible = isPointVisible(rect.bottom) &&
      isPointVisible(rect.bottom - (rect.height * MOSTLY_VISIBLE_PERCENT));
  const isMiddleMostlyVisible = rect.top < 0 &&
      rect.bottom > window.innerHeight &&
      (rect.height * MOSTLY_VISIBLE_PERCENT) < window.innerHeight;
  return isTopMostlyVisible || isBottomMostlyVisible || isMiddleMostlyVisible;
}

// Returns true if any part of the given rect is within the visible window.
export function isRectVisible(rect: DOMRect): boolean {
  return (rect.height > 0) &&
      ((rect.top <= 0 && rect.bottom >= window.innerHeight) ||
       isPointVisible(rect.top) || isPointVisible(rect.bottom));
}

function isPointVisible(point: number) {
  return (
      (point >= 0) &&
      ((point <= window.innerHeight) ||
       (point <= document.documentElement.clientHeight)));
}
