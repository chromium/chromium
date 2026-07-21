// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {isRTL} from '//resources/js/util.js';

import type {ShowAtConfigPrefs} from '../content/read_anything_types.js';
import {TextSegmenter} from '../read_aloud/text_segmenter.js';

// Determined by experimentation - can be adjusted to fine tune for different
// platforms.
export const minOverflowLengthToScroll = 75;
export const spinnerDebounceTimeout = 150;
export const playFromSelectionTimeout = spinnerDebounceTimeout + 25;

// How long to delay before logging the empty state. If it's only shown briefly,
// no need to log.
export const LOG_EMPTY_DELAY_MS = 500;

const ACTIVE_CSS_CLASS = 'active';

export function openMenu(
    menuToOpen: CrActionMenuElement, target: HTMLElement,
    showAtConfig?: ShowAtConfigPrefs, onShow?: () => void,
    isSubmenu: boolean = false) {
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

      // We manually override submenu positions here because cr-action-menu's
      // native side-collision aggressively flips the entire menu. We must do
      // this after showAt() because <cr-lazy-render> keeps offsetWidth at 0
      // until opened.
      if (isSubmenu) {
        const dialog = menuToOpen.getDialog();
        const targetRect = target.getBoundingClientRect();
        const viewportWidth = window.innerWidth;
        const menuWidth = dialog.offsetWidth;
        const idealLeft =
            isRTL() ? targetRect.right : targetRect.left - menuWidth;
        const maxLeftAllowed = viewportWidth - menuWidth;
        const exactLeft = Math.max(0, Math.min(idealLeft, maxLeftAllowed));

        dialog.style.left = `${exactLeft}px`;
        dialog.style.right = 'auto';
      }

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

// Returns true if the active distillation method is readability.
export function isDistilledByReadability(): boolean {
  return chrome.readingMode.activeDistillationMethod ===
      chrome.readingMode.distillationTypeReadability;
}
