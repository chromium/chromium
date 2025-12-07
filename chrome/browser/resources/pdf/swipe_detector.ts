// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

/**
 * The longest period of time in milliseconds for a horizontal touch movement to
 * be considered as a swipe.
 */
const SWIPE_TIMER_INTERVAL_MS: number = 200;

/* The minimum travel distance on the x axis for a swipe. */
const SWIPE_X_DIST_MIN: number = 150;

/* The maximum travel distance on the y axis for a swipe. */
const SWIPE_Y_DIST_MAX: number = 100;

export interface SwipeEvent {
  type: string;
  detail: SwipeDirection;
}

/** Enumeration of swipe directions. */
export enum SwipeDirection {
  RIGHT_TO_LEFT = 0,
  LEFT_TO_RIGHT = 1,
}

// A class that listens for touch events and produces events when these
// touches form swipe gestures.
export class SwipeDetector {
  private element_: HTMLElement;
  private isPresentationMode_: boolean = false;
  private swipeStartEvent_: TouchEvent|null = null;

  private elapsedTimeForTesting_: number|null = null;

  private eventTarget_: EventTarget = new EventTarget();

  /** @param element The element to monitor for touch gestures. */
  constructor(element: HTMLElement) {
    this.element_ = element;

    this.element_.addEventListener(
        'touchstart', (this.onTouchStart_.bind(this) as (p1: Event) => any),
        {passive: true});

    this.element_.addEventListener(
        'touchend', (this.onTouchEnd_.bind(this) as (p1: Event) => any),
        {passive: true});
    this.element_.addEventListener(
        'touchcancel', () => this.onTouchCancel_(), {passive: true});
  }

  /**
   * Public for tests. Allow manually setting the elapsed time for a swipe
   * action.
   */
  setElapsedTimerForTesting(time: number) {
    this.elapsedTimeForTesting_ = time;
  }

  setPresentationMode(enabled: boolean) {
    this.isPresentationMode_ = enabled;
  }

  getPresentationModeForTesting() {
    return this.isPresentationMode_;
  }

  getEventTarget(): EventTarget {
    return this.eventTarget_;
  }

  /**
   * Call the relevant listeners with the given swipe |direction|.
   * @param direction The direction of swipe action.
   */
  private notify_(direction: SwipeDirection) {
    this.eventTarget_.dispatchEvent(
        new CustomEvent('swipe', {detail: direction}));
  }

  /** The callback for touchstart events on the element. */
  private onTouchStart_(event: TouchEvent) {
    if (!this.isPresentationMode_) {
      return;
    }

    // If more than 1 finger touch the screen or there is already an ongoing
    // swipe detection process, there is no valid swipe event to keep track.
    if (event.touches.length !== 1 || this.swipeStartEvent_) {
      this.swipeStartEvent_ = null;
      return;
    }

    this.swipeStartEvent_ = event;
    return;
  }

  /** The callback for touchcancel events on the element. */
  private onTouchCancel_() {
    if (!this.isPresentationMode_ || !this.swipeStartEvent_) {
      return;
    }

    this.swipeStartEvent_ = null;
  }

  /** The callback for touchend events on the element. */
  private onTouchEnd_(event: TouchEvent) {
    if (!this.isPresentationMode_ || !this.swipeStartEvent_) {
      return;
    }

    if (event.touches.length !== 0 ||
        this.swipeStartEvent_.touches.length !== 1) {
      return;
    }

    const elapsedTime = this.elapsedTimeForTesting_ ?
        this.elapsedTimeForTesting_ :
        event.timeStamp - this.swipeStartEvent_.timeStamp;
    const swipeStartObj = this.swipeStartEvent_.changedTouches[0];
    assert(swipeStartObj);
    const swipeEndObj = event.changedTouches[0];
    assert(swipeEndObj);
    const distX = swipeEndObj.pageX - swipeStartObj.pageX;
    const distY = swipeEndObj.pageY - swipeStartObj.pageY;

    // If this is a valid swipe, notify its direction to the viewer.
    if (elapsedTime <= SWIPE_TIMER_INTERVAL_MS &&
        Math.abs(distX) >= SWIPE_X_DIST_MIN &&
        Math.abs(distY) <= SWIPE_Y_DIST_MAX) {
      const direction = distX > 0 ? SwipeDirection.LEFT_TO_RIGHT :
                                    SwipeDirection.RIGHT_TO_LEFT;
      this.notify_(direction);
    }

    this.swipeStartEvent_ = null;
  }
}
