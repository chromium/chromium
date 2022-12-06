// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DragWrapper} from 'chrome://resources/js/drag_wrapper.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getCardSlider} from './new_tab.js';
import {getCurrentlyDraggingTile} from './tile_page.js';

/**
 * @fileoverview Page switcher
 * This is the class for the left and right navigation arrows that switch
 * between pages.
 */

/**
 * @constructor
 * @extends {HTMLButtonElement}
 */
export function PageSwitcher() {}

PageSwitcher.prototype = {
  __proto__: HTMLButtonElement.prototype,

  /**
   * Activate the switcher (go to the next card).
   * @private
   */
  activate_() {
    getCardSlider().selectCard(this.nextCardIndex_(), true);
  },

  /**
   * Calculate the index of the card that this button will switch to.
   * @private
   */
  nextCardIndex_() {
    const cardSlider = getCardSlider();
    const index = cardSlider.currentCard + this.direction_;
    const numCards = cardSlider.cardCount - 1;
    return Math.max(0, Math.min(index, numCards));
  },

  /**
   * Update the accessible label attribute of this button, based on the
   * current position in the card slider and the names of the cards.
   * @param {NodeList} dots The dot elements which display the names of the
   *     cards.
   */
  updateButtonAccessibleLabel(dots) {
    const currentIndex = getCardSlider().currentCard;
    const nextCardIndex = this.nextCardIndex_();
    if (nextCardIndex === currentIndex) {
      this.setAttribute('aria-label', '');  // No next card.
      return;
    }

    const currentDot = dots[currentIndex];
    const nextDot = dots[nextCardIndex];
    if (!currentDot || !nextDot) {
      this.setAttribute('aria-label', '');  // Dots not initialised yet.
      return;
    }

    const currentPageTitle = currentDot.displayTitle;
    const nextPageTitle = nextDot.displayTitle;
    const msgName = (currentPageTitle === nextPageTitle) ?
        'page_switcher_same_title' :
        'page_switcher_change_title';
    const ariaLabel = loadTimeData.getStringF(msgName, nextPageTitle);
    this.setAttribute('aria-label', ariaLabel);
  },

  shouldAcceptDrag(e) {
    // Only allow page switching when a drop could happen on the page being
    // switched to.
    const nextPage = getCardSlider().getCardAtIndex(this.nextCardIndex_());
    return nextPage.shouldAcceptDrag(e);
  },

  doDragEnter(e) {
    this.scheduleDelayedSwitch_(e);
    this.doDragOver(e);
  },

  doDragLeave(e) {
    this.cancelDelayedSwitch_();
  },

  doDragOver(e) {
    e.preventDefault();
    const targetPage = getCardSlider().currentCardValue;
    if (targetPage.shouldAcceptDrag(e)) {
      targetPage.setDropEffect(e.dataTransfer);
    }
  },

  doDrop(e) {
    e.stopPropagation();
    this.cancelDelayedSwitch_();

    const tile = getCurrentlyDraggingTile();
    if (!tile) {
      return;
    }

    const sourcePage = tile.tilePage;
    const targetPage = getCardSlider().currentCardValue;
    if (targetPage === sourcePage || !targetPage.shouldAcceptDrag(e)) {
      return;
    }

    targetPage.appendDraggingTile();
  },

  /**
   * Starts a timer to activate the switcher. The timer repeats until
   * cancelled by cancelDelayedSwitch_.
   * @private
   */
  scheduleDelayedSwitch_(e) {
    // Stop switching when the next page can't be dropped onto.
    const nextPage = getCardSlider().getCardAtIndex(this.nextCardIndex_());
    if (!nextPage.shouldAcceptDrag(e)) {
      return;
    }

    const self = this;
    function navPageClearTimeout() {
      self.activate_();
      self.dragNavTimeout_ = null;
      self.scheduleDelayedSwitch_(e);
    }
    this.dragNavTimeout_ = window.setTimeout(navPageClearTimeout, 500);
  },

  /**
   * Cancels the timer that activates the switcher while dragging.
   * @private
   */
  cancelDelayedSwitch_() {
    if (this.dragNavTimeout_) {
      window.clearTimeout(this.dragNavTimeout_);
      this.dragNavTimeout_ = null;
    }
  },

};

export function initializePageSwitcher(el) {
  el.__proto__ = PageSwitcher.prototype;

  el.addEventListener('click', el.activate_);

  el.direction_ = el.id === 'page-switcher-start' ? -1 : 1;

  el.dragWrapper_ = new DragWrapper(el, el);
}
