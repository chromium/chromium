// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from 'chrome://resources/js/assert_ts.js';
import {isRTL} from 'chrome://resources/js/util_ts.js';

import {dispatchSimpleEvent} from './cr_deprecated.js';
import {TouchHandler} from './touch_handler.js';

/**
 * @fileoverview Card slider implementation. Allows you to create interactions
 * that have items that can slide left to right to reveal additional items.
 * Works by adding the necessary event handlers to a specific DOM structure
 * including a frame, container and cards.
 * - The frame defines the boundary of one item. Each card will be expanded to
 *   fill the width of the frame. This element is also overflow hidden so that
 *   the additional items left / right do not trigger horizontal scrolling.
 * - The container is what all the touch events are attached to. This element
 *   will be expanded to be the width of all cards.
 * - The cards are the individual viewable items. There should be one card for
 *   each item in the list. Only one card will be visible at a time. Two cards
 *   will be visible while you are transitioning between cards.
 *
 * This class is designed to work well on any hardware-accelerated touch device.
 * It should still work on pre-hardware accelerated devices it just won't feel
 * very good. It should also work well with a mouse.
 */

// Use an anonymous function to enable strict mode just for this file (which
// will be concatenated with other files when embedded in Chrome
/**
 * @constructor
 * @param {!Element} frame The bounding rectangle that cards are visible in.
 * @param {!Element} container The surrounding element that will have event
 *     listeners attached to it.
 * @param {number} cardWidth The width of each card should have.
 */
export function CardSlider(frame, container, cardWidth) {
  /**
   * @type {!Element}
   * @private
   */
  this.frame_ = frame;

  /**
   * @type {!Element}
   * @private
   */
  this.container_ = container;

  /**
   * Array of card elements.
   * @type {!Array<!Element>}
   * @private
   */
  this.cards_ = [];

  /**
   * Index of currently shown card.
   * @type {number}
   * @private
   */
  this.currentCard_ = -1;

  /**
   * @type {number}
   * @private
   */
  this.cardWidth_ = cardWidth;

  /**
   * @type {!TouchHandler}
   * @private
   */
  this.touchHandler_ = new TouchHandler(this.container_);
}


/**
 * The time to transition between cards when animating. Measured in ms.
 * @type {number}
 * @private
 * @const
 */
CardSlider.TRANSITION_TIME_ = 200;


/**
 * The minimum velocity required to transition cards if they did not drag past
 * the halfway point between cards. Measured in pixels / ms.
 * @type {number}
 * @private
 * @const
 */
CardSlider.TRANSITION_VELOCITY_THRESHOLD_ = 0.2;


CardSlider.prototype = {
  /**
   * The current left offset of the container relative to the frame. This
   * position does not include deltas from active drag operations, and
   * always aligns with a frame boundary.
   * @type {number}
   * @private
   */
  currentLeft_: 0,

  /**
   * Current offset relative to |currentLeft_| due to an active drag
   * operation.
   * @type {number}
   * @private
   */
  deltaX_: 0,

  /**
   * Initialize all elements and event handlers. Must call after construction
   * and before usage.
   * @param {boolean} ignoreMouseWheelEvents If true, horizontal mouse wheel
   *     events will be ignored, rather than flipping between pages.
   */
  initialize(ignoreMouseWheelEvents) {
    const view = this.container_.ownerDocument.defaultView;
    assert(
        view.getComputedStyle(this.container_).display === '-webkit-box',
        'Container should be display -webkit-box.');
    assert(
        view.getComputedStyle(this.frame_).overflow === 'hidden',
        'Frame should be overflow hidden.');
    assert(
        view.getComputedStyle(this.container_).position === 'static',
        'Container should be position static.');

    this.updateCardWidths_();

    this.mouseWheelScrollAmount_ = 0;
    this.mouseWheelCardSelected_ = false;
    this.mouseWheelIsContinuous_ = false;
    this.scrollClearTimeout_ = null;
    if (!ignoreMouseWheelEvents) {
      this.frame_.addEventListener('mousewheel', this.onMouseWheel_.bind(this));
    }
    this.container_.addEventListener(
        'transitionend', this.onTransitionEnd_.bind(this));

    // Also support touch events in case a touch screen happens to be
    // available.  Note that this has minimal impact in the common case of
    // no touch events (eg. we're mainly just adding listeners for events that
    // will never trigger).
    this.container_.addEventListener(
        TouchHandler.EventType.TOUCH_START, this.onTouchStart_.bind(this));
    this.container_.addEventListener(
        TouchHandler.EventType.DRAG_START, this.onDragStart_.bind(this));
    this.container_.addEventListener(
        TouchHandler.EventType.DRAG_MOVE, this.onDragMove_.bind(this));
    this.container_.addEventListener(
        TouchHandler.EventType.DRAG_END, this.onDragEnd_.bind(this));

    this.touchHandler_.enable(/* opt_capture */ false);
  },

  /**
   * Use in cases where the width of the frame has changed in order to update
   * the width of cards. For example should be used when orientation changes
   * in full width sliders.
   * @param {number} newCardWidth Width all cards should have, in pixels.
   */
  resize(newCardWidth) {
    if (newCardWidth !== this.cardWidth_) {
      this.cardWidth_ = newCardWidth;

      this.updateCardWidths_();

      // Must upate the transform on the container to show the correct card.
      this.transformToCurrentCard_();
    }
  },

  /**
   * Sets the cards used. Can be called more than once to switch card sets.
   * @param {!Array<!Element>} cards The individual viewable cards.
   * @param {number} index Index of the card to in the new set of cards to
   *     navigate to.
   */
  setCards(cards, index) {
    assert(
        index >= 0 && index < cards.length,
        'Invalid index in CardSlider#setCards');
    this.cards_ = cards;

    this.updateCardWidths_();
    this.updateSelectedCardAttributes_();

    // Jump to the given card index.
    this.selectCard(index, false, false, true);
  },

  /**
   * Ensures that for all cards:
   * - if the card is the current card, then it has 'selected-card' in its
   *   classList, and is visible for accessibility
   * - if the card is not the selected card, then it does not have
   *   'selected-card' in its classList, and is invisible for accessibility.
   * @private
   */
  updateSelectedCardAttributes_() {
    for (let i = 0; i < this.cards_.length; i++) {
      if (i === this.currentCard_) {
        this.cards_[i].classList.add('selected-card');
        this.cards_[i].removeAttribute('aria-hidden');
      } else {
        this.cards_[i].classList.remove('selected-card');
        this.cards_[i].setAttribute('aria-hidden', true);
      }
    }
  },

  /**
   * Updates the width of each card.
   * @private
   */
  updateCardWidths_() {
    for (let i = 0, card; card = this.cards_[i]; i++) {
      card.style.width = this.cardWidth_ + 'px';
    }
  },

  /**
   * Returns the index of the current card.
   * @return {number} index of the current card.
   */
  get currentCard() {
    return this.currentCard_;
  },

  /**
   * Allows setting the current card index.
   * @param {number} index A new index to set the current index to.
   * @return {number} The new index after having been set.
   */
  set currentCard(index) {
    return (this.currentCard_ = index);
  },

  /**
   * Returns the number of cards.
   * @return {number} number of cards.
   */
  get cardCount() {
    return this.cards_.length;
  },

  /**
   * Returns the current card itself.
   * @return {!Element} the currently shown card.
   */
  get currentCardValue() {
    return this.cards_[this.currentCard_];
  },

  /**
   * Returns the frame holding the cards.
   * @return {Element} The frame used to position the cards.
   */
  get frame() {
    return this.frame_;
  },

  /**
   * Handle horizontal scrolls to flip between pages.
   * @private
   */
  onMouseWheel_(e) {
    if (e.wheelDeltaX === 0) {
      return;
    }

    // Continuous devices such as an Apple Touchpad or Apple MagicMouse will
    // send arbitrary delta values. Conversly, standard mousewheels will
    // send delta values in increments of 120.  (There is of course a small
    // chance we mistake a continuous device for a non-continuous device.
    // Unfortunately there isn't a better way to do this until real touch
    // events are available to desktop clients.)
    const DISCRETE_DELTA = 120;
    if (e.wheelDeltaX % DISCRETE_DELTA) {
      this.mouseWheelIsContinuous_ = true;
    }

    if (this.mouseWheelIsContinuous_) {
      // For continuous devices, detect a page swipe when the accumulated
      // delta matches a pre-defined threshhold.  After changing the page,
      // ignore wheel events for a short time before repeating this process.
      if (this.mouseWheelCardSelected_) {
        return;
      }
      this.mouseWheelScrollAmount_ += e.wheelDeltaX;
      if (Math.abs(this.mouseWheelScrollAmount_) >= 600) {
        let pagesToScroll = this.mouseWheelScrollAmount_ > 0 ? 1 : -1;
        if (!isRTL()) {
          pagesToScroll *= -1;
        }
        let newCardIndex = this.currentCard + pagesToScroll;
        newCardIndex =
            Math.min(this.cards_.length - 1, Math.max(0, newCardIndex));
        this.selectCard(newCardIndex, true);
        this.mouseWheelCardSelected_ = true;
      }
    } else {
      // For discrete devices, consider each wheel tick a page change.
      let pagesToScroll = e.wheelDeltaX / DISCRETE_DELTA;
      if (!isRTL()) {
        pagesToScroll *= -1;
      }
      let newCardIndex = this.currentCard + pagesToScroll;
      newCardIndex =
          Math.min(this.cards_.length - 1, Math.max(0, newCardIndex));
      this.selectCard(newCardIndex, true);
    }

    // We got a mouse wheel event, so cancel any pending scroll wheel timeout.
    if (this.scrollClearTimeout_ !== null) {
      clearTimeout(this.scrollClearTimeout_);
    }
    // If we didn't use up all the scroll, hold onto it for a little bit, but
    // drop it after a delay.
    if (this.mouseWheelScrollAmount_ !== 0) {
      this.scrollClearTimeout_ =
          setTimeout(this.clearMouseWheelScroll_.bind(this), 500);
    }
  },

  /**
   * Resets the amount of horizontal scroll we've seen to 0. See
   * onMouseWheel_.
   * @private
   */
  clearMouseWheelScroll_() {
    this.mouseWheelScrollAmount_ = 0;
    this.mouseWheelCardSelected_ = false;
  },

  /**
   * Handles the ends of transitions on transform (animated
   * card switches).
   * @param {Event} e The transitionend event.
   * @private
   */
  onTransitionEnd_(e) {
    // Ignore irrelevant transitions that might bubble up.
    if (e.target !== this.container_ || e.propertyName !== 'transform') {
      return;
    }
    this.fireChangeEndedEvent_(true);
  },

  /**
   * Dispatches a simple event to tell subscribers we're done moving to the
   * newly selected card.
   * @param {boolean} wasAnimated whether or not the change was animated.
   * @private
   */
  fireChangeEndedEvent_(wasAnimated) {
    const e = document.createEvent('Event');
    e.initEvent('cardSlider:card_change_ended', true, true);
    e.cardSlider = this;
    e.changedTo = this.currentCard_;
    e.wasAnimated = wasAnimated;
    this.container_.dispatchEvent(e);
  },

  /**
   * Add a card to the card slider at a particular index. If the card being
   * added is inserted in front of the current card, cardSlider.currentCard
   * will be adjusted accordingly (to current card + 1).
   * @param {!Node} card A card that will be added to the card slider.
   * @param {number} index An index at which the given |card| should be
   *     inserted. Must be positive and less than the number of cards.
   */
  addCardAtIndex(card, index) {
    assert(card instanceof Node, '|card| isn\'t a Node');
    this.assertValidIndex_(index);
    this.cards_ = Array.prototype.concat.call(
        this.cards_.slice(0, index), card, this.cards_.slice(index));

    this.updateSelectedCardAttributes_();

    if (this.currentCard_ === -1) {
      this.currentCard_ = 0;
    } else if (index <= this.currentCard_) {
      this.selectCard(this.currentCard_ + 1, false, true, true);
    }

    this.fireAddedEvent_(card, index);
  },

  /**
   * Append a card to the end of the list.
   * @param {!Element} card A card to add at the end of the card slider.
   */
  appendCard(card) {
    assertInstanceof(card, Element);
    this.cards_.push(card);
    this.fireAddedEvent_(card, this.cards_.length - 1);
  },

  /**
   * Dispatches a simple event to tell interested subscribers that a card was
   * added to this card slider.
   * @param {Node} card The recently added card.
   * @param {number} index The position of the newly added card.
   * @private
   */
  fireAddedEvent_(card, index) {
    this.assertValidIndex_(index);
    const e = document.createEvent('Event');
    e.initEvent('cardSlider:card_added', true, true);
    e.addedIndex = index;
    e.addedCard = card;
    this.container_.dispatchEvent(e);
  },

  /**
   * Returns the card at a particular index.
   * @param {number} index The index of the card to return.
   * @return {!Element} The card at the given index.
   */
  getCardAtIndex(index) {
    this.assertValidIndex_(index);
    return this.cards_[index];
  },

  /**
   * Removes a card by index from the card slider. If the card to be removed
   * is the current card or in front of the current card, the current card
   * will be updated (to current card - 1).
   * @param {!Element} card A card to be removed.
   */
  removeCard(card) {
    assertInstanceof(card, Element);
    this.removeCardAtIndex(this.cards_.indexOf(card));
  },

  /**
   * Removes a card by index from the card slider. If the card to be removed
   * is the current card or in front of the current card, the current card
   * will be updated (to current card - 1).
   * @param {number} index The index of the tile that should be removed.
   */
  removeCardAtIndex(index) {
    this.assertValidIndex_(index);
    const removed = this.cards_.splice(index, 1).pop();

    if (this.cards_.length === 0) {
      this.currentCard_ = -1;
    } else if (index < this.currentCard_) {
      this.selectCard(this.currentCard_ - 1, false, true);
    }

    this.fireRemovedEvent_(removed, index);
  },

  /**
   * Dispatches a cardSlider:card_removed event so interested subscribers know
   * when a card was removed from this card slider.
   * @param {Node} card The recently removed card.
   * @param {number} index The index of the card before it was removed.
   * @private
   */
  fireRemovedEvent_(card, index) {
    const e = document.createEvent('Event');
    e.initEvent('cardSlider:card_removed', true, true);
    e.removedCard = card;
    e.removedIndex = index;
    this.container_.dispatchEvent(e);
  },

  /**
   * This re-syncs the transform that's used to position the frame in
   * the likely event it needs to be updated by a card being inserted or
   * removed in the flow.
   */
  repositionFrame() {
    this.transformToCurrentCard_();
  },

  /**
   * Checks the the given |index| exists in this.cards_.
   * @param {number} index An index to check.
   * @private
   */
  assertValidIndex_(index) {
    assert(index >= 0 && index < this.cards_.length);
  },

  /**
   * Selects a new card, ensuring that it is a valid index, transforming the
   * view and possibly calling the change card callback.
   * @param {number} newCardIndex Index of card to show.
   * @param {boolean=} opt_animate If true will animate transition from
   *     current position to new position.
   * @param {boolean=} opt_dontNotify If true, don't tell subscribers that
   *     we've changed cards.
   * @param {boolean=} opt_forceChange If true, ignore if the card already
   *     selected.
   */
  selectCard(newCardIndex, opt_animate, opt_dontNotify, opt_forceChange) {
    this.assertValidIndex_(newCardIndex);

    const previousCard = this.currentCardValue;
    let isChangingCard =
        !this.cards_[newCardIndex].classList.contains('selected-card');

    if (typeof opt_forceChange !== 'undefined' && opt_forceChange) {
      isChangingCard = true;
    }

    if (isChangingCard) {
      this.currentCard_ = newCardIndex;
      this.updateSelectedCardAttributes_();
    }

    const willTransitionHappen = this.transformToCurrentCard_(opt_animate);

    if (isChangingCard && !opt_dontNotify) {
      const event = document.createEvent('Event');
      event.initEvent('cardSlider:card_changed', true, true);
      event.cardSlider = this;
      event.wasAnimated = !!opt_animate;
      this.container_.dispatchEvent(event);

      // We also dispatch an event on the cards themselves.
      if (previousCard) {
        dispatchSimpleEvent(previousCard, 'carddeselected', true, true);
      }
      dispatchSimpleEvent(this.currentCardValue, 'cardselected', true, true);
    }

    // If we're not changing, animated, or transitioning, fire a
    // cardSlider:card_change_ended event right away.
    if ((!isChangingCard || !opt_animate || !willTransitionHappen) &&
        !opt_dontNotify) {
      this.fireChangeEndedEvent_(false);
    }
  },

  /**
   * Selects a card from the stack. Passes through to selectCard.
   * @param {!Element} newCard The card that should be selected.
   * @param {boolean=} opt_animate Whether to animate.
   */
  selectCardByValue(newCard, opt_animate) {
    const i = this.cards_.indexOf(newCard);
    assert(i !== -1);
    this.selectCard(i, opt_animate);
  },

  /**
   * Centers the view on the card denoted by this.currentCard. Can either
   * animate to that card or snap to it.
   * @param {boolean=} opt_animate If true will animate transition from
   *     current position to new position.
   * @return {boolean} Whether or not a transformation was necessary.
   * @private
   */
  transformToCurrentCard_(opt_animate) {
    const prevLeft = this.currentLeft_;
    this.currentLeft_ = -this.cardWidth_ *
        (isRTL() ? this.cards_.length - this.currentCard - 1 :
                   this.currentCard);

    // If there's no change, return something to let the caller know there
    // won't be a transition occuring.
    if (prevLeft === this.currentLeft_ && this.deltaX_ === 0) {
      return false;
    }

    // Animate to the current card, which will either transition if the
    // current card is new, or reset the existing card if we didn't drag
    // enough to change cards.
    let transition = '';
    if (opt_animate) {
      transition =
          'transform ' + CardSlider.TRANSITION_TIME_ + 'ms ease-in-out';
    }
    this.container_.style.transition = transition;
    this.translateTo_(this.currentLeft_);

    return true;
  },

  /**
   * Moves the view to the specified position.
   * @param {number} x Horizontal position to move to.
   * @private
   */
  translateTo_(x) {
    // We use a transform to slide because this is GPU accelerated on
    // Chrome and iOS.  Once Chrome does GPU acceleration on the position
    // fixed-layout elements we could simply set the element's position to
    // fixed and modify 'left' instead.
    this.deltaX_ = x - this.currentLeft_;
    this.container_.style.transform = 'translate3d(' + x + 'px, 0, 0)';
  },

  /* Touch ******************************************************************/

  /**
   * Clear any transition that is in progress and enable dragging for the
   * touch.
   * @param {!Event} e The TouchHandler event.
   * @private
   */
  onTouchStart_(e) {
    e = /** @type {!TouchHandler.Event} */ (e);
    this.container_.style.transition = '';
    e.enableDrag = true;
  },

  /**
   * Tell the TouchHandler that dragging is acceptable when the user begins by
   * scrolling horizontally and there is more than one card to slide.
   * @param {!Event} e The TouchHandler event.
   * @private
   */
  onDragStart_(e) {
    e = /** @type {!TouchHandler.Event} */ (e);
    e.enableDrag =
        this.cardCount > 1 && Math.abs(e.dragDeltaX) > Math.abs(e.dragDeltaY);
  },

  /**
   * On each drag move event reposition the container appropriately so the
   * cards look like they are sliding.
   * @param {!Event} e The TouchHandler event.
   * @private
   */
  onDragMove_(e) {
    e = /** @type {!TouchHandler.Event} */ (e);
    let deltaX = e.dragDeltaX;
    // If dragging beyond the first or last card then apply a backoff so the
    // dragging feels stickier than usual.
    if (!this.currentCard && deltaX > 0 ||
        this.currentCard === (this.cards_.length - 1) && deltaX < 0) {
      deltaX /= 2;
    }
    this.translateTo_(this.currentLeft_ + deltaX);
  },

  /**
   * On drag end events we may want to transition to another card, depending
   * on the ending position of the drag and the velocity of the drag.
   * @param {!Event} e The TouchHandler event.
   * @private
   */
  onDragEnd_(e) {
    e = /** @type {!TouchHandler.Event} */ (e);
    const deltaX = e.dragDeltaX;
    const velocity = this.touchHandler_.getEndVelocity().x;
    const newX = this.currentLeft_ + deltaX;
    let newCardIndex = Math.round(-newX / this.cardWidth_);

    if (newCardIndex === this.currentCard &&
        Math.abs(velocity) > CardSlider.TRANSITION_VELOCITY_THRESHOLD_) {
      // The drag wasn't far enough to change cards but the velocity was
      // high enough to transition anyways. If the velocity is to the left
      // (negative) then the user wishes to go right (card + 1).
      newCardIndex += velocity > 0 ? -1 : 1;
    }
    // Ensure that the new card index is valid.  The new card index could be
    // invalid if a swipe suggests scrolling off the end of the list of
    // cards.
    if (newCardIndex < 0) {
      newCardIndex = 0;
    } else if (newCardIndex >= this.cardCount) {
      newCardIndex = this.cardCount - 1;
    }
    this.selectCard(newCardIndex, /* animate */ true);
  },

  /**
   * Cancel any current touch/slide as if we saw a touch end
   */
  cancelTouch() {
    // Stop listening to any current touch
    this.touchHandler_.cancelTouch();

    // Ensure we're at a card bounary
    this.transformToCurrentCard_(true);
  },
};
