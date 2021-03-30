// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';
import {Point} from './constants.js';

/**
 * @typedef {{
 *   center: !Point,
 *   direction: (string|undefined),
 *   scaleRatio: (?number|undefined),
 *   startScaleRatio: (?number|undefined),
 * }}
 */
export let PinchEventDetail;

// A class that listens for touch events and produces events when these
// touches form gestures (e.g. pinching).
export class GestureDetector {
  /**
   * @param {!EventTarget|!Element} element The element to monitor for touch
   *     gestures.
   */
  constructor(element) {
    element.addEventListener(
        'touchstart',
        /** @type {function(!Event)} */ (this.onTouchStart_.bind(this)),
        {passive: true});

    const boundOnTouch =
        /** @type {function(!Event)} */ (this.onTouch_.bind(this));
    element.addEventListener('touchmove', boundOnTouch, {passive: true});
    element.addEventListener('touchend', boundOnTouch, {passive: true});
    element.addEventListener('touchcancel', boundOnTouch, {passive: true});

    element.addEventListener(
        'wheel',
        /** @type {function(!Event)} */ (this.onWheel_.bind(this)),
        {passive: false});
    document.addEventListener(
        'contextmenu', e => this.handleContextMenuEvent_(e));

    this.pinchStartEvent_ = null;
    this.lastTouchTouchesCount_ = 0;

    /** @private {TouchEvent} */
    this.lastEvent_ = null;

    /**
     * The scale relative to the start of the pinch when handling ctrl-wheels.
     * null when there is no ongoing pinch.
     *
     * @private {?number}
     */
    this.accumulatedWheelScale_ = null;
    /**
     * A timeout ID from setTimeout used for sending the pinchend event when
     * handling ctrl-wheels.
     *
     * @private {?number}
     */
    this.wheelEndTimeout_ = null;

    /** @private {!EventTarget} */
    this.eventTarget_ = new EventTarget();
  }

  /** @return {!EventTarget} */
  getEventTarget() {
    return this.eventTarget_;
  }

  /**
   * Public for tests.
   * @return {boolean} True if the last touch start was a two finger touch.
   */
  wasTwoFingerTouch() {
    return this.lastTouchTouchesCount_ === 2;
  }

  /**
   * Call the relevant listeners with the given |PinchEventDetail|.
   * @param {string} type The type of pinch event.
   * @param {!PinchEventDetail} detail The event to notify the listeners of.
   * @private
   */
  notify_(type, detail) {
    this.eventTarget_.dispatchEvent(new CustomEvent(type, {detail}));
  }

  /**
   * The callback for touchstart events on the element.
   * @param {!TouchEvent} event Touch event on the element.
   * @private
   */
  onTouchStart_(event) {
    this.lastTouchTouchesCount_ = event.touches.length;
    if (!this.wasTwoFingerTouch()) {
      return;
    }

    this.pinchStartEvent_ = event;
    this.lastEvent_ = event;
    this.notify_('pinchstart', {center: center(event)});
  }

  /**
   * The callback for touch move, end, and cancel events on the element.
   * @param {!TouchEvent} event Touch event on the element.
   * @private
   */
  onTouch_(event) {
    if (!this.pinchStartEvent_) {
      return;
    }

    const lastEvent = /** @type {!TouchEvent} */ (this.lastEvent_);

    // Check if the pinch ends with the current event.
    if (event.touches.length < 2 ||
        lastEvent.touches.length !== event.touches.length) {
      const startScaleRatio = pinchScaleRatio(lastEvent, this.pinchStartEvent_);
      this.pinchStartEvent_ = null;
      this.lastEvent_ = null;
      this.notify_('pinchend', {
        startScaleRatio: startScaleRatio,
        center: center(lastEvent),
      });
      return;
    }

    const scaleRatio = pinchScaleRatio(event, lastEvent);
    const startScaleRatio = pinchScaleRatio(event, this.pinchStartEvent_);
    this.notify_('pinchupdate', {
      scaleRatio: scaleRatio,
      direction: scaleRatio > 1.0 ? 'in' : 'out',
      startScaleRatio: startScaleRatio,
      center: center(event),
    });

    this.lastEvent_ = event;
  }

  /**
   * The callback for wheel events on the element.
   * @param {!WheelEvent} event Wheel event on the element.
   * @private
   */
  onWheel_(event) {
    // We handle ctrl-wheels to invoke our own pinch zoom. On Mac, synthetic
    // ctrl-wheels are created from trackpad pinches. We handle these ourselves
    // to prevent the browser's native pinch zoom. We also use our pinch
    // zooming mechanism for handling non-synthetic ctrl-wheels. This allows us
    // to anchor the zoom around the mouse position instead of the scroll
    // position.
    if (!event.ctrlKey) {
      return;
    }

    event.preventDefault();

    // Disable wheel gestures in Presentation mode.
    if (document.fullscreenElement !== null) {
      return;
    }

    const wheelScale = Math.exp(-event.deltaY / 100);
    // Clamp scale changes from the wheel event as they can be
    // quite dramatic for non-synthetic ctrl-wheels.
    const scale = Math.min(1.25, Math.max(0.75, wheelScale));
    const position = {x: event.clientX, y: event.clientY};

    if (this.accumulatedWheelScale_ == null) {
      this.accumulatedWheelScale_ = 1.0;
      this.notify_('pinchstart', {center: position});
    }

    this.accumulatedWheelScale_ *= scale;
    this.notify_('pinchupdate', {
      scaleRatio: scale,
      direction: scale > 1.0 ? 'in' : 'out',
      startScaleRatio: this.accumulatedWheelScale_,
      center: position,
    });

    // We don't get any phase information for the ctrl-wheels, so we don't know
    // when the gesture ends. We'll just use a timeout to send the pinch end
    // event a short time after the last ctrl-wheel we see.
    if (this.wheelEndTimeout_ != null) {
      window.clearTimeout(this.wheelEndTimeout_);
      this.wheelEndTimeout_ = null;
    }
    const gestureEndDelayMs = 100;
    const endEvent = {
      startScaleRatio: this.accumulatedWheelScale_,
      center: position,
    };
    this.wheelEndTimeout_ = window.setTimeout(() => {
      this.notify_('pinchend', endEvent);
      this.wheelEndTimeout_ = null;
      this.accumulatedWheelScale_ = null;
    }, gestureEndDelayMs);
  }

  /**
   * @param {!Event} e The context menu event
   * @private
   */
  handleContextMenuEvent_(e) {
    // Stop Chrome from popping up the context menu on long press. We need to
    // make sure the start event did not have 2 touches because we don't want
    // to block two finger tap opening the context menu. We check for
    // firesTouchEvents in order to not block the context menu on right click.
    const capabilities =
        /** @type {{ sourceCapabilities: Object }} */ (e).sourceCapabilities;
    if (capabilities.firesTouchEvents && !this.wasTwoFingerTouch()) {
      e.preventDefault();
    }
  }
}

/**
 * Computes the change in scale between this touch event
 * and a previous one.
 * @param {!TouchEvent} event Latest touch event on the element.
 * @param {!TouchEvent} prevEvent A previous touch event on the element.
 * @return {?number} The ratio of the scale of this event and the
 *     scale of the previous one.
 */
function pinchScaleRatio(event, prevEvent) {
  const distance1 = distance(prevEvent);
  const distance2 = distance(event);
  return distance1 === 0 ? null : distance2 / distance1;
}

/**
 * Computes the distance between fingers.
 * @param {!TouchEvent} event Touch event with at least 2 touch points.
 * @return {number} Distance between touch[0] and touch[1].
 */
function distance(event) {
  const touch1 = event.touches[0];
  const touch2 = event.touches[1];
  const dx = touch1.clientX - touch2.clientX;
  const dy = touch1.clientY - touch2.clientY;
  return Math.sqrt(dx * dx + dy * dy);
}

/**
 * Computes the midpoint between fingers.
 * @param {!TouchEvent} event Touch event with at least 2 touch points.
 * @return {!Point} Midpoint between touch[0] and touch[1].
 */
function center(event) {
  const touch1 = event.touches[0];
  const touch2 = event.touches[1];
  return {
    x: (touch1.clientX + touch2.clientX) / 2,
    y: (touch1.clientY + touch2.clientY) / 2
  };
}

// Export on |window| such that scripts injected from pdf_extension_test.cc can
// access it.
window.GestureDetector = GestureDetector;
