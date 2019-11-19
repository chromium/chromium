// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A class that listens for touch events and produces events when these
 * touches form gestures (e.g. pinching).
 */
export class GestureDetector {
  /**
   * @param {!Element} element The element to monitor for touch gestures.
   */
  constructor(element) {
    /** @private {!Element} */
    this.element_ = element;

    this.element_.addEventListener(
        'touchstart',
        /** @type {function(!Event)} */ (this.onTouchStart_.bind(this)),
        {passive: true});

    const boundOnTouch =
        /** @type {function(!Event)} */ (this.onTouch_.bind(this));
    this.element_.addEventListener('touchmove', boundOnTouch, {passive: true});
    this.element_.addEventListener('touchend', boundOnTouch, {passive: true});
    this.element_.addEventListener(
        'touchcancel', boundOnTouch, {passive: true});

    this.element_.addEventListener(
        'wheel',
        /** @type {function(!Event)} */ (this.onWheel_.bind(this)),
        {passive: false});

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

    /** @private {!Map<string, !Array<!Function>>} */
    this.listeners_ =
        new Map([['pinchstart', []], ['pinchupdate', []], ['pinchend', []]]);
  }

  /**
   * Add a |listener| to be notified of |type| events.
   *
   * @param {string} type The event type to be notified for.
   * @param {!Function} listener The callback.
   */
  addEventListener(type, listener) {
    if (this.listeners_.has(type)) {
      this.listeners_.get(type).push(listener);
    }
  }

  /**
   * @return {boolean} True if the last touch start was a two finger touch.
   */
  wasTwoFingerTouch() {
    return this.lastTouchTouchesCount_ == 2;
  }

  /**
   * Call the relevant listeners with the given |pinchEvent|.
   *
   * @param {!Object} pinchEvent The event to notify the listeners of.
   * @private
   */
  notify_(pinchEvent) {
    const listeners = this.listeners_.get(pinchEvent.type);

    for (const l of listeners) {
      l(pinchEvent);
    }
  }

  /**
   * The callback for touchstart events on the element.
   *
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
    this.notify_({type: 'pinchstart', center: GestureDetector.center_(event)});
  }

  /**
   * The callback for touch move, end, and cancel events on the element.
   *
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
      const startScaleRatio =
          GestureDetector.pinchScaleRatio_(lastEvent, this.pinchStartEvent_);
      const center = GestureDetector.center_(lastEvent);
      const endEvent = {
        type: 'pinchend',
        startScaleRatio: startScaleRatio,
        center: center
      };
      this.pinchStartEvent_ = null;
      this.lastEvent_ = null;
      this.notify_(endEvent);
      return;
    }

    const scaleRatio = GestureDetector.pinchScaleRatio_(event, lastEvent);
    const startScaleRatio =
        GestureDetector.pinchScaleRatio_(event, this.pinchStartEvent_);
    const center = GestureDetector.center_(event);
    this.notify_({
      type: 'pinchupdate',
      scaleRatio: scaleRatio,
      direction: scaleRatio > 1.0 ? 'in' : 'out',
      startScaleRatio: startScaleRatio,
      center: center
    });

    this.lastEvent_ = event;
  }

  /**
   * The callback for wheel events on the element.
   *
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

    const wheelScale = Math.exp(-event.deltaY / 100);
    // Clamp scale changes from the wheel event as they can be
    // quite dramatic for non-synthetic ctrl-wheels.
    const scale = Math.min(1.25, Math.max(0.75, wheelScale));
    const position = {x: event.clientX, y: event.clientY};

    if (this.accumulatedWheelScale_ == null) {
      this.accumulatedWheelScale_ = 1.0;
      this.notify_({type: 'pinchstart', center: position});
    }

    this.accumulatedWheelScale_ *= scale;
    this.notify_({
      type: 'pinchupdate',
      scaleRatio: scale,
      direction: scale > 1.0 ? 'in' : 'out',
      startScaleRatio: this.accumulatedWheelScale_,
      center: position
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
      type: 'pinchend',
      startScaleRatio: this.accumulatedWheelScale_,
      center: position
    };
    this.wheelEndTimeout_ = window.setTimeout(function(endEvent) {
      this.notify_(endEvent);
      this.wheelEndTimeout_ = null;
      this.accumulatedWheelScale_ = null;
    }.bind(this), gestureEndDelayMs, endEvent);
  }

  /**
   * Computes the change in scale between this touch event
   * and a previous one.
   *
   * @param {!TouchEvent} event Latest touch event on the element.
   * @param {!TouchEvent} prevEvent A previous touch event on the element.
   * @return {?number} The ratio of the scale of this event and the
   *     scale of the previous one.
   * @private
   */
  static pinchScaleRatio_(event, prevEvent) {
    const distance1 = GestureDetector.distance_(prevEvent);
    const distance2 = GestureDetector.distance_(event);
    return distance1 === 0 ? null : distance2 / distance1;
  }

  /**
   * Computes the distance between fingers.
   *
   * @param {!TouchEvent} event Touch event with at least 2 touch points.
   * @return {number} Distance between touch[0] and touch[1].
   * @private
   */
  static distance_(event) {
    const touch1 = event.touches[0];
    const touch2 = event.touches[1];
    const dx = touch1.clientX - touch2.clientX;
    const dy = touch1.clientY - touch2.clientY;
    return Math.sqrt(dx * dx + dy * dy);
  }

  /**
   * Computes the midpoint between fingers.
   *
   * @param {!TouchEvent} event Touch event with at least 2 touch points.
   * @return {!Object} Midpoint between touch[0] and touch[1].
   * @private
   */
  static center_(event) {
    const touch1 = event.touches[0];
    const touch2 = event.touches[1];
    return {
      x: (touch1.clientX + touch2.clientX) / 2,
      y: (touch1.clientY + touch2.clientY) / 2
    };
  }
}

// Export on |window| such that scripts injected from pdf_extension_test.cc can
// access it.
window.GestureDetector = GestureDetector;
