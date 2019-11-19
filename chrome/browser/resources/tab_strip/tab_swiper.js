// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The minimum amount of pixels needed for the user to swipe for the opacity
 * to start animating to 0.
 * @const {number}
 */
export const OPACITY_ANIMATION_THRESHOLD_PX = 30;

/**
 * The minimum amount of pixels needed for the user to swipe to actually close
 * the tab. This also triggers animating other properties to suggest more that
 * the tab will close, such as animating the max-width.
 * @const {number}
 */
export const SWIPE_START_THRESHOLD_PX = 100;

/**
 * The maximum amount of pixels needed to swipe a tab away. This is how many
 * pixels across the screen the user needs to swipe for the swipe away
 * animation to complete such that the tab is gone from the screen.
 * @const {number}
 */
export const SWIPE_FINISH_THRESHOLD_PX = 200;

/**
 * The pixel that maps to the time of the swipe away animation at which the tab
 * is in its original stable position.
 * @const {number}
 */
const SWIPE_ANIMATION_BASELINE_PX = SWIPE_FINISH_THRESHOLD_PX;

/**
 * The swipe away animation is bidirectional to allow the user to swipe in
 * either direction, so the total span of pixels is SWIPE_FINISH_THRESHOLD_PX in
 * both directions.
 * @const {number}
 */
const SWIPE_ANIMATION_TOTAL_PX = SWIPE_FINISH_THRESHOLD_PX * 2;

/**
 * The minimum velocity of pixels per milliseconds required for the tab to
 * register the set of pointer events as an intended swipe.
 * @const {number}
 */
const SWIPE_VELOCITY_THRESHOLD = 0.1;

export class TabSwiper {
  /** @param {!HTMLElement} element */
  constructor(element) {
    /** @private @const {!HTMLElement} */
    this.element_ = element;

    /** @private @const {!Animation} */
    this.animation_ = this.createAnimation_();

    /**
     * Whether any part of the animation that updates properties has begun since
     * the last pointerdown event.
     * @private {boolean}
     */
    this.animationInitiated_ = false;

    /** @private {?PointerEvent} */
    this.currentPointerDownEvent_ = null;

    /** @private @const {!Function} */
    this.pointerDownListener_ = e =>
        this.onPointerDown_(/** @type {!PointerEvent} */ (e));

    /** @private @const {!Function} */
    this.pointerMoveListener_ = e =>
        this.onPointerMove_(/** @type {!PointerEvent} */ (e));

    /** @private @const {!Function} */
    this.pointerLeaveListener_ = e =>
        this.onPointerLeave_(/** @type {!PointerEvent} */ (e));

    /** @private @const {!Function} */
    this.pointerUpListener_ = e =>
        this.onPointerUp_(/** @type {!PointerEvent} */ (e));
  }

  /** @private */
  clearPointerEvents_() {
    this.currentPointerDownEvent_ = null;
    this.element_.removeEventListener(
        'pointerleave', this.pointerLeaveListener_);
    this.element_.removeEventListener('pointermove', this.pointerMoveListener_);
    this.element_.removeEventListener('pointerup', this.pointerUpListener_);
  }

  /** @private */
  createAnimation_() {
    const animation = new Animation(new KeyframeEffect(
        this.element_,
        [
          {
            // Fully swiped up.
            maxWidth: '0px',
            opacity: 0,
            transform: `translateY(-${SWIPE_FINISH_THRESHOLD_PX}px)`
          },
          {
            // Start of max-width animation swiping up.
            maxWidth: 'var(--tabstrip-tab-width)',
            offset: (SWIPE_ANIMATION_BASELINE_PX - SWIPE_START_THRESHOLD_PX) /
                SWIPE_ANIMATION_TOTAL_PX,
          },
          {
            // Start of opacity animation swiping up.
            maxWidth: 'var(--tabstrip-tab-width)',
            offset:
                (SWIPE_ANIMATION_BASELINE_PX - OPACITY_ANIMATION_THRESHOLD_PX) /
                SWIPE_ANIMATION_TOTAL_PX,
            opacity: 1,
            transform: `translateY(0)`
          },
          {
            // Base.
            opacity: 1,
            maxWidth: 'var(--tabstrip-tab-width)',
            transform: `translateY(0)`
          },
          {
            // Start of opacity animation swiping down.
            maxWidth: 'var(--tabstrip-tab-width)',
            offset:
                (SWIPE_ANIMATION_BASELINE_PX + OPACITY_ANIMATION_THRESHOLD_PX) /
                SWIPE_ANIMATION_TOTAL_PX,
            opacity: 1,
            transform: `translateY(0)`
          },
          {
            // Start of opacity animation swiping down.
            maxWidth: 'var(--tabstrip-tab-width)',
            offset: (SWIPE_ANIMATION_BASELINE_PX + SWIPE_START_THRESHOLD_PX) /
                SWIPE_ANIMATION_TOTAL_PX,
          },
          {
            // Fully swiped down.
            maxWidth: '0px',
            opacity: 0,
            transform: `translateY(${SWIPE_FINISH_THRESHOLD_PX}px)`
          },
        ],
        {
          duration: SWIPE_ANIMATION_TOTAL_PX,
          fill: 'both',
        }));
    animation.currentTime = SWIPE_FINISH_THRESHOLD_PX;
    animation.onfinish = () => {
      this.element_.dispatchEvent(new CustomEvent('swipe'));
    };
    return animation;
  }

  /**
   * @param {!PointerEvent} event
   * @private
   */
  onPointerDown_(event) {
    if (this.currentPointerDownEvent_) {
      return;
    }

    this.animation_.currentTime = SWIPE_ANIMATION_BASELINE_PX;
    this.animationInitiated_ = false;
    this.currentPointerDownEvent_ = event;

    this.element_.addEventListener('pointerleave', this.pointerLeaveListener_);
    this.element_.addEventListener('pointermove', this.pointerMoveListener_);
    this.element_.addEventListener('pointerup', this.pointerUpListener_);
  }

  /**
   * @param {!PointerEvent} event
   * @private
   */
  onPointerLeave_(event) {
    if (this.currentPointerDownEvent_.pointerId !== event.pointerId) {
      return;
    }

    this.clearPointerEvents_();
  }

  /**
   * @param {!PointerEvent} event
   * @private
   */
  onPointerMove_(event) {
    if (this.currentPointerDownEvent_.pointerId !== event.pointerId ||
        event.movementY === 0) {
      return;
    }

    const yDiff = event.clientY - this.currentPointerDownEvent_.clientY;
    const animationTime = SWIPE_ANIMATION_BASELINE_PX + yDiff;
    this.animation_.currentTime =
        Math.max(0, Math.min(SWIPE_ANIMATION_TOTAL_PX, animationTime));

    if (!this.animationInitiated_ &&
        Math.abs(yDiff) > OPACITY_ANIMATION_THRESHOLD_PX) {
      this.animationInitiated_ = true;
      this.element_.setPointerCapture(event.pointerId);
    }
  }

  /**
   * @param {!PointerEvent} event
   * @private
   */
  onPointerUp_(event) {
    if (this.currentPointerDownEvent_.pointerId !== event.pointerId) {
      return;
    }

    const pixelsSwiped =
        this.animation_.currentTime - SWIPE_ANIMATION_BASELINE_PX;
    const swipedEnoughToClose =
        Math.abs(pixelsSwiped) > SWIPE_START_THRESHOLD_PX;
    const wasHighVelocity =
        Math.abs(
            pixelsSwiped /
            (event.timeStamp - this.currentPointerDownEvent_.timeStamp)) >
        SWIPE_VELOCITY_THRESHOLD;

    if (Math.abs(pixelsSwiped) === SWIPE_FINISH_THRESHOLD_PX) {
      // The user has swiped the max amount of pixels to swipe and the animation
      // has already completed all its keyframes, so just fire the onfinish
      // events on the animation.
      this.animation_.finish();
    } else if (swipedEnoughToClose || wasHighVelocity) {
      this.animation_.playbackRate = Math.sign(pixelsSwiped);
      this.animation_.play();
    } else {
      this.animation_.cancel();
      this.animation_.currentTime = SWIPE_FINISH_THRESHOLD_PX;
    }

    this.clearPointerEvents_();
  }

  startObserving() {
    this.element_.addEventListener('pointerdown', this.pointerDownListener_);
  }

  stopObserving() {
    this.element_.removeEventListener('pointerdown', this.pointerDownListener_);
  }

  /** @return {boolean} */
  wasSwiping() {
    return this.animationInitiated_;
  }
}
