// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isRTL} from 'chrome://resources/js/util.js';

/**
 * The minimum amount of pixels needed for the user to swipe for the position
 * (controlled by transform property) to start animating to 0.
 */
export const TRANSLATE_ANIMATION_THRESHOLD_PX: number = 30;

/**
 * The minimum amount of pixels needed for the user to swipe to actually close
 * the tab. This also triggers animating other properties to suggest more that
 * the tab will close, such as animating the max-width.
 */
export const SWIPE_START_THRESHOLD_PX: number = 100;

/**
 * The maximum amount of pixels needed to swipe a tab away. This is how many
 * pixels across the screen the user needs to swipe for the swipe away
 * animation to complete such that the tab is gone from the screen.
 * TODO(johntlee): Make this relative to the height of the tab, not a
 * hard-coded value.
 */
export const SWIPE_FINISH_THRESHOLD_PX: number = 200;

/**
 * The minimum velocity of pixels per milliseconds required for the tab to
 * register the set of pointer events as an intended swipe.
 */
const SWIPE_VELOCITY_THRESHOLD: number = 0.2;

export class TabSwiper {
  private element_: HTMLElement;
  private animation_: Animation;
  private animationInitiated_: boolean;
  private currentPointerDownEvent_: PointerEvent|null = null;

  private pointerDownListener_: (e: PointerEvent) => void;
  private pointerMoveListener_: (e: PointerEvent) => void;
  private pointerLeaveListener_: (e: PointerEvent) => void;
  private pointerUpListener_: (e: PointerEvent) => void;

  constructor(element: HTMLElement) {
    this.element_ = element;

    this.animation_ = this.createAnimation_();

    /**
     * Whether any part of the animation that updates properties has begun since
     * the last pointerdown event.
     */
    this.animationInitiated_ = false;

    this.pointerDownListener_ = e => this.onPointerDown_(e);

    this.pointerMoveListener_ = e => this.onPointerMove_(e);

    this.pointerLeaveListener_ = e => this.onPointerLeave_(e);

    this.pointerUpListener_ = e => this.onPointerUp_(e);
  }

  private clearPointerEvents_() {
    this.currentPointerDownEvent_ = null;
    this.element_.removeEventListener(
        'pointerleave', this.pointerLeaveListener_);
    this.element_.removeEventListener('pointermove', this.pointerMoveListener_);
    this.element_.removeEventListener('pointerup', this.pointerUpListener_);
  }

  private createAnimation_() {
    // TODO(crbug.com/40659171): padding-inline-end does not work with
    // animations built using JS.
    const paddingInlineEnd = isRTL() ? 'paddingLeft' : 'paddingRight';
    const animation = new Animation(new KeyframeEffect(
        this.element_,
        [
          {
            // Base.
            opacity: 1,
            maxWidth: 'var(--tabstrip-tab-width)',
            [paddingInlineEnd]: 'var(--tabstrip-tab-spacing)',
            transform: `translateY(0)`,
          },
          {
            // Start of transform animation swiping up.
            offset:
                TRANSLATE_ANIMATION_THRESHOLD_PX / SWIPE_FINISH_THRESHOLD_PX,
            transform: `translateY(0)`,
          },
          {
            // Start of max-width and opacity animation swiping up.
            maxWidth: 'var(--tabstrip-tab-width)',
            offset: SWIPE_START_THRESHOLD_PX / SWIPE_FINISH_THRESHOLD_PX,
            [paddingInlineEnd]: 'var(--tabstrip-tab-spacing)',
            opacity: 1,
          },
          {
            // Fully swiped up.
            maxWidth: '0px',
            opacity: 0,
            [paddingInlineEnd]: 0,
            transform: `translateY(-${SWIPE_FINISH_THRESHOLD_PX}px)`,
          },
        ],
        {
          duration: SWIPE_FINISH_THRESHOLD_PX,
          fill: 'both',
        }));
    animation.cancel();
    animation.onfinish = () => {
      this.element_.dispatchEvent(new CustomEvent('swipe'));
    };
    return animation;
  }

  private onPointerDown_(event: PointerEvent) {
    if (this.currentPointerDownEvent_ || event.pointerType !== 'touch') {
      return;
    }

    this.animation_.currentTime = 0;
    this.animationInitiated_ = false;
    this.currentPointerDownEvent_ = event;

    this.element_.addEventListener('pointerleave', this.pointerLeaveListener_);
    this.element_.addEventListener('pointermove', this.pointerMoveListener_);
    this.element_.addEventListener('pointerup', this.pointerUpListener_);
  }

  private onPointerLeave_(event: PointerEvent) {
    if (this.currentPointerDownEvent_!.pointerId !== event.pointerId) {
      return;
    }

    this.clearPointerEvents_();
  }

  private onPointerMove_(event: PointerEvent) {
    if (this.currentPointerDownEvent_!.pointerId !== event.pointerId ||
        event.movementY === 0) {
      return;
    }

    const yDiff = this.currentPointerDownEvent_!.clientY - event.clientY;
    const animationTime = yDiff;
    this.animation_.currentTime =
        Math.max(0, Math.min(SWIPE_FINISH_THRESHOLD_PX, animationTime));

    if (!this.animationInitiated_ &&
        Math.abs(yDiff) > TRANSLATE_ANIMATION_THRESHOLD_PX) {
      this.animationInitiated_ = true;
      this.element_.setPointerCapture(event.pointerId);
    }
  }

  private onPointerUp_(event: PointerEvent) {
    if (this.currentPointerDownEvent_!.pointerId !== event.pointerId) {
      return;
    }

    const pixelsSwiped: number = Number(this.animation_.currentTime!);
    const swipedEnoughToClose = pixelsSwiped > SWIPE_START_THRESHOLD_PX;
    const wasHighVelocity = pixelsSwiped /
            (event.timeStamp - this.currentPointerDownEvent_!.timeStamp) >
        SWIPE_VELOCITY_THRESHOLD;

    if (pixelsSwiped === SWIPE_FINISH_THRESHOLD_PX) {
      // The user has swiped the max amount of pixels to swipe and the animation
      // has already completed all its keyframes, so just fire the onfinish
      // events on the animation.
      this.animation_.finish();
    } else if (swipedEnoughToClose || wasHighVelocity) {
      this.animation_.play();
    } else {
      this.animation_.cancel();
      this.animation_.currentTime = 0;
    }

    this.clearPointerEvents_();
  }

  reset() {
    this.animation_.cancel();
  }

  startObserving() {
    this.element_.addEventListener('pointerdown', this.pointerDownListener_);
  }

  stopObserving() {
    this.element_.removeEventListener('pointerdown', this.pointerDownListener_);
  }

  wasSwiping(): boolean {
    return this.animationInitiated_;
  }
}
