// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from 'chrome://resources/js/assert_ts.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';

/**
 * @fileoverview Touch Handler. Class that handles all touch events and
 * uses them to interpret higher level gestures and behaviors. TouchEvent is a
 * built in mobile safari type:
 * http://developer.apple.com/safari/library/documentation/UserExperience/Reference/TouchEventClassReference/TouchEvent/TouchEvent.html.
 * This class is intended to work with all webkit browsers, tested on Chrome and
 * iOS.
 *
 * The following types of gestures are currently supported.  See the definition
 * of TouchHandler.EventType for details.
 *
 * Single Touch:
 *      This provides simple single-touch events.  Any secondary touch is
 *      ignored.
 *
 * Drag:
 *      A single touch followed by some movement. This behavior will handle all
 *      of the required events and report the properties of the drag to you
 *      while the touch is happening and at the end of the drag sequence. This
 *      behavior will NOT perform the actual dragging (redrawing the element)
 *      for you, this responsibility is left to the client code.
 *
 * Long press:
 *     When your element is touched and held without any drag occuring, the
 *     LONG_PRESS event will fire.
 */


/**
 * A TouchHandler attaches to an Element, listents for low-level touch (or
 * mouse) events and dispatching higher-level events on the element.
 * @param {!Element} element The element to listen on and fire events
 * for.
 * @constructor
 */
export function TouchHandler(element) {
  /**
   * @type {!Element}
   * @private
   */
  this.element_ = element;

  /**
   * The absolute sum of all touch y deltas.
   * @type {number}
   * @private
   */
  this.totalMoveY_ = 0;

  /**
   * The absolute sum of all touch x deltas.
   * @type {number}
   * @private
   */
  this.totalMoveX_ = 0;

  /**
   * An array of tuples where the first item is the horizontal component of a
   * recent relevant touch and the second item is the touch's time stamp. Old
   * touches are removed based on the max tracking time and when direction
   * changes.
   * @type {!Array<number>}
   * @private
   */
  this.recentTouchesX_ = [];

  /**
   * An array of tuples where the first item is the vertical component of a
   * recent relevant touch and the second item is the touch's time stamp. Old
   * touches are removed based on the max tracking time and when direction
   * changes.
   * @type {!Array<number>}
   * @private
   */
  this.recentTouchesY_ = [];

  /**
   * Used to keep track of all events we subscribe to so we can easily clean
   * up
   * @type {EventTracker}
   * @private
   */
  this.events_ = new EventTracker();
}


/**
 * DOM Events that may be fired by the TouchHandler at the element
 * @enum {string}
 */
TouchHandler.EventType = {
  // Fired whenever the element is touched as the only touch to the device.
  // enableDrag defaults to false, set to true to permit dragging.
  TOUCH_START: 'touchHandler:touch_start',

  // Fired when an element is held for a period of time.  Prevents dragging
  // from occuring (even if enableDrag was set to true).
  LONG_PRESS: 'touchHandler:long_press',

  // If enableDrag was set to true at TOUCH_START, DRAG_START will fire when
  // the touch first moves sufficient distance.  enableDrag is set to true but
  // can be reset to false to cancel the drag.
  DRAG_START: 'touchHandler:drag_start',

  // If enableDrag was true after DRAG_START, DRAG_MOVE will fire whenever the
  // touch is moved.
  DRAG_MOVE: 'touchHandler:drag_move',

  // Fired just before TOUCH_END when a drag is released.  Correlates 1:1 with
  // a DRAG_START.
  DRAG_END: 'touchHandler:drag_end',

  // Fired whenever a touch that is being tracked has been released.
  // Correlates 1:1 with a TOUCH_START.
  TOUCH_END: 'touchHandler:touch_end',

  // Fired whenever the element is tapped in a short time and no dragging is
  // detected.
  TAP: 'touchHandler:tap',
};


/**
 * The type of event sent by TouchHandler
 * @constructor
 * @extends {Event}
 * @param {!TouchHandler.EventType} type The type of event.
 * @param {boolean} bubbles Whether or not the event should bubble.
 * @param {number} clientX The X location of the touch.
 * @param {number} clientY The Y location of the touch.
 * @param {!Element} touchedElement The element at the current location of the
 *        touch.
 */
TouchHandler.Event = function(type, bubbles, clientX, clientY, touchedElement) {
  const event = document.createEvent('Event');
  event.initEvent(type, bubbles, true);
  event.__proto__ = TouchHandler.Event.prototype;

  /**
   * The X location of the touch affected
   * @type {number}
   */
  event.clientX = clientX;

  /**
   * The Y location of the touch affected
   * @type {number}
   */
  event.clientY = clientY;

  /**
   * The element at the current location of the touch.
   * @type {!Element}
   */
  event.touchedElement = touchedElement;

  return event;
};

TouchHandler.Event.prototype = {
  __proto__: Event.prototype,

  /**
   * For TOUCH_START and DRAG START events, set to true to enable dragging or
   * false to disable dragging.
   * @type {boolean|undefined}
   */
  enableDrag: undefined,

  /**
   * For DRAG events, provides the horizontal component of the
   * drag delta. Drag delta is defined as the delta of the start touch
   * position and the current drag position.
   * @type {number|undefined}
   */
  dragDeltaX: undefined,

  /**
   * For DRAG events, provides the vertical component of the
   * drag delta.
   * @type {number|undefined}
   */
  dragDeltaY: undefined,
};

/**
 * Maximum movement of touch required to be considered a tap.
 * @type {number}
 * @private
 */
TouchHandler.MAX_TRACKING_FOR_TAP_ = 8;

/**
 * The maximum number of ms to track a touch event. After an event is older
 * than this value, it will be ignored in velocity calculations.
 * @type {number}
 * @private
 */
TouchHandler.MAX_TRACKING_TIME_ = 250;

/**
 * The maximum number of touches to track.
 * @type {number}
 * @private
 */
TouchHandler.MAX_TRACKING_TOUCHES_ = 5;

/**
 * The maximum velocity to return, in pixels per millisecond, that is used
 * to guard against errors in calculating end velocity of a drag. This is a
 * very fast drag velocity.
 * @type {number}
 * @private
 */
TouchHandler.MAXIMUM_VELOCITY_ = 5;

/**
 * The velocity to return, in pixel per millisecond, when the time stamps on
 * the events are erroneous. The browser can return bad time stamps if the
 * thread is blocked for the duration of the drag. This is a low velocity to
 * prevent the content from moving quickly after a slow drag. It is less
 * jarring if the content moves slowly after a fast drag.
 * @type {number}
 * @private
 */
TouchHandler.VELOCITY_FOR_INCORRECT_EVENTS_ = 1;

/**
 * The time, in milliseconds, that a touch must be held to be considered
 * 'long'.
 * @type {number}
 * @private
 */
TouchHandler.TIME_FOR_LONG_PRESS_ = 500;

TouchHandler.prototype = {
  /**
   * If defined, the identifer of the single touch that is active.  Note that
   * 0 is a valid touch identifier - it should not be treated equivalently to
   * undefined.
   * @type {number|undefined}
   * @private
   */
  activeTouch_: undefined,

  /**
   * @type {boolean|undefined}
   * @private
   */
  tracking_: undefined,

  /**
   * @type {number|undefined}
   * @private
   */
  startTouchX_: undefined,

  /**
   * @type {number|undefined}
   * @private
   */
  startTouchY_: undefined,

  /**
   * @type {number|undefined}
   * @private
   */
  endTouchX_: undefined,

  /**
   * @type {number|undefined}
   * @private
   */
  endTouchY_: undefined,

  /**
   * Time of the touchstart event.
   * @type {number|undefined}
   * @private
   */
  startTime_: undefined,

  /**
   * The time of the touchend event.
   * @type {number|undefined}
   * @private
   */
  endTime_: undefined,

  /**
   * @type {number|undefined}
   * @private
   */
  lastTouchX_: undefined,

  /**
   * @type {number|undefined}
   * @private
   */
  lastTouchY_: undefined,

  /**
   * @type {number|undefined}
   * @private
   */
  lastMoveX_: undefined,

  /**
   * @type {number|undefined}
   * @private
   */
  lastMoveY_: undefined,

  /**
   * @type {number|undefined}
   * @private
   */
  longPressTimeout_: undefined,

  /**
   * If defined and true, the next click event should be swallowed
   * @type {boolean|undefined}
   * @private
   */
  swallowNextClick_: undefined,

  /**
   * @type {boolean}
   * @private
   */
  draggingEnabled_: false,

  /**
   * Start listenting for events.
   * @param {boolean=} opt_capture True if the TouchHandler should listen to
   *      during the capture phase.
   * @param {boolean=} opt_mouse True if the TouchHandler should generate
   *      events for mouse input (in addition to touch input).
   */
  enable(opt_capture, opt_mouse) {
    const capture = !!opt_capture;

    // Just listen to start events for now. When a touch is occuring we'll
    // want to be subscribed to move and end events on the document, but we
    // don't want to incur the cost of lots of no-op handlers on the document.
    this.events_.add(
        this.element_, 'touchstart', this.onStart_.bind(this), capture);
    if (opt_mouse) {
      this.events_.add(
          this.element_, 'mousedown',
          this.mouseToTouchCallback_(this.onStart_.bind(this)), capture);
    }

    // If the element is long-pressed, we may need to swallow a click
    this.events_.add(this.element_, 'click', this.onClick_.bind(this), true);
  },

  /**
   * Stop listening to all events.
   */
  disable() {
    this.stopTouching_();
    this.events_.removeAll();
  },

  /**
   * Wraps a callback with translations of mouse events to touch events.
   * NOTE: These types really should be function(Event) but then we couldn't
   * use this with bind (which operates on any type of function).  Doesn't
   * JSDoc support some sort of polymorphic types?
   * @param {Function} callback The event callback.
   * @return {Function} The wrapping callback.
   * @private
   */
  mouseToTouchCallback_(callback) {
    return function(e) {
      // Note that there may be synthesizes mouse events caused by touch
      // events (a mouseDown after a touch-click).  We leave it up to the
      // client to worry about this if it matters to them (typically a short
      // mouseDown/mouseUp without a click is no big problem and it's not
      // obvious how we identify such synthesized events in a general way).
      const touch = {
        // any fixed value will do for the identifier - there will only
        // ever be a single active 'touch' when using the mouse.
        identifier: 0,
        clientX: e.clientX,
        clientY: e.clientY,
        target: e.target,
      };
      e.touches = [];
      e.targetTouches = [];
      e.changedTouches = [touch];
      if (e.type !== 'mouseup') {
        e.touches[0] = touch;
        e.targetTouches[0] = touch;
      }
      callback(e);
    };
  },

  /**
   * Begin tracking the touchable element, it is eligible for dragging.
   * @private
   */
  beginTracking_() {
    this.tracking_ = true;
  },

  /**
   * Stop tracking the touchable element, it is no longer dragging.
   * @private
   */
  endTracking_() {
    this.tracking_ = false;
    this.dragging_ = false;
    this.totalMoveY_ = 0;
    this.totalMoveX_ = 0;
  },

  /**
   * Reset the touchable element as if we never saw the touchStart
   * Doesn't dispatch any end events - be careful of existing listeners.
   */
  cancelTouch() {
    this.stopTouching_();
    this.endTracking_();
    // If clients needed to be aware of this, we could fire a cancel event
    // here.
  },

  /**
   * Record that touching has stopped
   * @private
   */
  stopTouching_() {
    // Mark as no longer being touched
    this.activeTouch_ = undefined;

    // If we're waiting for a long press, stop
    window.clearTimeout(this.longPressTimeout_);

    // Stop listening for move/end events until there's another touch.
    // We don't want to leave handlers piled up on the document.
    // Note that there's no harm in removing handlers that weren't added, so
    // rather than track whether we're using mouse or touch we do both.
    this.events_.remove(document, 'touchmove');
    this.events_.remove(document, 'touchend');
    this.events_.remove(document, 'touchcancel');
    this.events_.remove(document, 'mousemove');
    this.events_.remove(document, 'mouseup');
  },

  /**
   * Touch start handler.
   * @param {!TouchEvent} e The touchstart event.
   * @private
   */
  onStart_(e) {
    // Only process single touches.  If there is already a touch happening, or
    // two simultaneous touches then just ignore them.
    if (e.touches.length > 1) {
      // Note that we could cancel an active touch here.  That would make
      // simultaneous touch behave similar to near-simultaneous. However, if
      // the user is dragging something, an accidental second touch could be
      // quite disruptive if it cancelled their drag.  Better to just ignore
      // it.
      return;
    }

    // It's still possible there could be an active "touch" if the user is
    // simultaneously using a mouse and a touch input.
    if (this.activeTouch_ !== undefined) {
      return;
    }

    const touch = e.targetTouches[0];
    this.activeTouch_ = touch.identifier;

    // We've just started touching so shouldn't swallow any upcoming click
    if (this.swallowNextClick_) {
      this.swallowNextClick_ = false;
    }

    this.disableTap_ = false;

    // Sign up for end/cancel notifications for this touch.
    // Note that we do this on the document so that even if the user drags
    // their finger off the element, we'll still know what they're doing.
    if (e.type === 'mousedown') {
      this.events_.add(
          document, 'mouseup',
          this.mouseToTouchCallback_(this.onEnd_.bind(this)), false);
    } else {
      this.events_.add(document, 'touchend', this.onEnd_.bind(this), false);
      this.events_.add(document, 'touchcancel', this.onEnd_.bind(this), false);
    }

    // This timeout is cleared on touchEnd and onDrag
    // If we invoke the function then we have a real long press
    window.clearTimeout(this.longPressTimeout_);
    this.longPressTimeout_ = window.setTimeout(
        this.onLongPress_.bind(this), TouchHandler.TIME_FOR_LONG_PRESS_);

    // Dispatch the TOUCH_START event
    this.draggingEnabled_ =
        !!this.dispatchEvent_(TouchHandler.EventType.TOUCH_START, touch);

    // We want dragging notifications
    if (e.type === 'mousedown') {
      this.events_.add(
          document, 'mousemove',
          this.mouseToTouchCallback_(this.onMove_.bind(this)), false);
    } else {
      this.events_.add(document, 'touchmove', this.onMove_.bind(this), false);
    }

    this.startTouchX_ = this.lastTouchX_ = touch.clientX;
    this.startTouchY_ = this.lastTouchY_ = touch.clientY;
    this.startTime_ = e.timeStamp;

    this.recentTouchesX_ = [];
    this.recentTouchesY_ = [];
    this.recentTouchesX_.push(touch.clientX, e.timeStamp);
    this.recentTouchesY_.push(touch.clientY, e.timeStamp);

    this.beginTracking_();
  },

  /**
   * Given a list of Touches, find the one matching our activeTouch
   * identifier. Note that Chrome currently always uses 0 as the identifier.
   * In that case we'll end up always choosing the first element in the list.
   * @param {TouchList} touches The list of Touch objects to search.
   * @return {!Touch|undefined} The touch matching our active ID if any.
   * @private
   */
  findActiveTouch_(touches) {
    assert(this.activeTouch_ !== undefined, 'Expecting an active touch');
    // A TouchList isn't actually an array, so we shouldn't use
    // Array.prototype.filter/some, etc.
    for (let i = 0; i < touches.length; i++) {
      if (touches[i].identifier === this.activeTouch_) {
        return touches[i];
      }
    }
    return undefined;
  },

  /**
   * Touch move handler.
   * @param {!TouchEvent} e The touchmove event.
   * @private
   */
  onMove_(e) {
    if (!this.tracking_) {
      return;
    }

    // Our active touch should always be in the list of touches still active
    assert(this.findActiveTouch_(e.touches), 'Missing touchEnd');

    const that = this;
    const touch = this.findActiveTouch_(e.changedTouches);
    if (!touch) {
      return;
    }

    const clientX = touch.clientX;
    const clientY = touch.clientY;

    const moveX = this.lastTouchX_ - clientX;
    const moveY = this.lastTouchY_ - clientY;
    this.totalMoveX_ += Math.abs(moveX);
    this.totalMoveY_ += Math.abs(moveY);
    this.lastTouchX_ = clientX;
    this.lastTouchY_ = clientY;

    const couldBeTap = this.totalMoveY_ <= TouchHandler.MAX_TRACKING_FOR_TAP_ ||
        this.totalMoveX_ <= TouchHandler.MAX_TRACKING_FOR_TAP_;

    if (!couldBeTap) {
      this.disableTap_ = true;
    }

    if (this.draggingEnabled_ && !this.dragging_ && !couldBeTap) {
      // If we're waiting for a long press, stop
      window.clearTimeout(this.longPressTimeout_);

      // Dispatch the DRAG_START event and record whether dragging should be
      // allowed or not.  Note that this relies on the current value of
      // startTouchX/Y - handlers may use the initial drag delta to determine
      // if dragging should be permitted.
      this.dragging_ =
          this.dispatchEvent_(TouchHandler.EventType.DRAG_START, touch);

      if (this.dragging_) {
        // Update the start position here so that drag deltas have better
        // values but don't touch the recent positions so that velocity
        // calculations can still use touchstart position in the time and
        // distance delta.
        this.startTouchX_ = clientX;
        this.startTouchY_ = clientY;
        this.startTime_ = e.timeStamp;
      } else {
        this.endTracking_();
      }
    }

    if (this.dragging_) {
      this.dispatchEvent_(TouchHandler.EventType.DRAG_MOVE, touch);

      this.removeTouchesInWrongDirection_(
          this.recentTouchesX_, this.lastMoveX_, moveX);
      this.removeTouchesInWrongDirection_(
          this.recentTouchesY_, this.lastMoveY_, moveY);
      this.removeOldTouches_(this.recentTouchesX_, e.timeStamp);
      this.removeOldTouches_(this.recentTouchesY_, e.timeStamp);
      this.recentTouchesX_.push(clientX, e.timeStamp);
      this.recentTouchesY_.push(clientY, e.timeStamp);
    }

    this.lastMoveX_ = moveX;
    this.lastMoveY_ = moveY;
  },

  /**
   * Filters the provided recent touches array to remove all touches except
   * the last if the move direction has changed.
   * @param {!Array<number>} recentTouches An array of tuples where the first
   *     item is the x or y component of the recent touch and the second item
   *     is the touch time stamp.
   * @param {number|undefined} lastMove The x or y component of the previous
   *     move.
   * @param {number} recentMove The x or y component of the most recent move.
   * @private
   */
  removeTouchesInWrongDirection_(recentTouches, lastMove, recentMove) {
    if (lastMove && recentMove && recentTouches.length > 2 &&
        (lastMove > 0 ^ recentMove > 0)) {
      recentTouches.splice(0, recentTouches.length - 2);
    }
  },

  /**
   * Filters the provided recent touches array to remove all touches older
   * than the max tracking time or the 5th most recent touch.
   * @param {!Array<number>} recentTouches An array of tuples where the first
   *     item is the x or y component of the recent touch and the second item
   *     is the touch time stamp.
   * @param {number} recentTime The time of the most recent event.
   * @private
   */
  removeOldTouches_(recentTouches, recentTime) {
    while (recentTouches.length &&
               recentTime - recentTouches[1] >
                   TouchHandler.MAX_TRACKING_TIME_ ||
           recentTouches.length > TouchHandler.MAX_TRACKING_TOUCHES_ * 2) {
      recentTouches.splice(0, 2);
    }
  },

  /**
   * Touch end handler.
   * @param {!TouchEvent} e The touchend event.
   * @private
   */
  onEnd_(e) {
    const that = this;
    assert(this.activeTouch_ !== undefined, 'Expect to already be touching');

    // If the touch we're tracking isn't changing here, ignore this touch end.
    const touch = this.findActiveTouch_(e.changedTouches);
    if (!touch) {
      // In most cases, our active touch will be in the 'touches' collection,
      // but we can't assert that because occasionally two touchend events can
      // occur at almost the same time with both having empty 'touches' lists.
      // I.e., 'touches' seems like it can be a bit more up to date than the
      // current event.
      return;
    }

    // This is touchEnd for the touch we're monitoring
    assert(!this.findActiveTouch_(e.touches), 'Touch ended also still active');

    // Indicate that touching has finished
    this.stopTouching_();

    if (this.tracking_) {
      const clientX = touch.clientX;
      const clientY = touch.clientY;

      if (this.dragging_) {
        this.endTime_ = e.timeStamp;
        this.endTouchX_ = clientX;
        this.endTouchY_ = clientY;

        this.removeOldTouches_(this.recentTouchesX_, e.timeStamp);
        this.removeOldTouches_(this.recentTouchesY_, e.timeStamp);

        this.dispatchEvent_(TouchHandler.EventType.DRAG_END, touch);

        // Note that in some situations we can get a click event here as well.
        // For now this isn't a problem, but we may want to consider having
        // some logic that hides clicks that appear to be caused by a touchEnd
        // used for dragging.
      }

      this.endTracking_();
    }
    this.draggingEnabled_ = false;

    // Note that we dispatch the touchEnd event last so that events at
    // different levels of semantics nest nicely (similar to how DOM
    // drag-and-drop events are nested inside of the mouse events that trigger
    // them).
    this.dispatchEvent_(TouchHandler.EventType.TOUCH_END, touch);
    if (!this.disableTap_) {
      this.dispatchEvent_(TouchHandler.EventType.TAP, touch);
    }
  },

  /**
   * Get end velocity of the drag. This method is specific to drag behavior,
   * so if touch behavior and drag behavior is split then this should go with
   * drag behavior. End velocity is defined as deltaXY / deltaTime where
   * deltaXY is the difference between endPosition and the oldest recent
   * position, and deltaTime is the difference between endTime and the oldest
   * recent time stamp.
   * @return {Object} The x and y velocity.
   */
  getEndVelocity() {
    // Note that we could move velocity to just be an end-event parameter.
    let velocityX = this.recentTouchesX_.length ?
        (this.endTouchX_ - this.recentTouchesX_[0]) /
            (this.endTime_ - this.recentTouchesX_[1]) :
        0;
    let velocityY = this.recentTouchesY_.length ?
        (this.endTouchY_ - this.recentTouchesY_[0]) /
            (this.endTime_ - this.recentTouchesY_[1]) :
        0;

    velocityX = this.correctVelocity_(velocityX);
    velocityY = this.correctVelocity_(velocityY);

    return {x: velocityX, y: velocityY};
  },

  /**
   * Correct erroneous velocities by capping the velocity if we think it's too
   * high, or setting it to a default velocity if know that the event data is
   * bad.
   * @param {number} velocity The x or y velocity component.
   * @return {number} The corrected velocity.
   * @private
   */
  correctVelocity_(velocity) {
    let absVelocity = Math.abs(velocity);

    // We add to recent touches for each touchstart and touchmove. If we have
    // fewer than 3 touches (6 entries), we assume that the thread was blocked
    // for the duration of the drag and we received events in quick succession
    // with the wrong time stamps.
    if (absVelocity > TouchHandler.MAXIMUM_VELOCITY_) {
      absVelocity = this.recentTouchesY_.length < 3 ?
          TouchHandler.VELOCITY_FOR_INCORRECT_EVENTS_ :
          TouchHandler.MAXIMUM_VELOCITY_;
    }
    return absVelocity * (velocity < 0 ? -1 : 1);
  },

  /**
   * Handler when an element has been pressed for a long time
   * @private
   */
  onLongPress_() {
    // Swallow any click that occurs on this element without an intervening
    // touch start event.  This simple click-busting technique should be
    // sufficient here since a real click should have a touchstart first.
    this.swallowNextClick_ = true;
    this.disableTap_ = true;

    // Dispatch to the LONG_PRESS
    assert(typeof this.startTouchX_ === 'number');
    assert(typeof this.startTouchY_ === 'number');
    this.dispatchEventXY_(
        TouchHandler.EventType.LONG_PRESS, this.element_,
        /** @type {number} */ (this.startTouchX_),
        /** @type {number} */ (this.startTouchY_));
  },

  /**
   * Click handler - used to swallow clicks after a long-press
   * @param {!Event} e The click event.
   * @private
   */
  onClick_(e) {
    if (this.swallowNextClick_) {
      e.preventDefault();
      e.stopPropagation();
      this.swallowNextClick_ = false;
    }
  },

  /**
   * Dispatch a TouchHandler event to the element
   * @param {!TouchHandler.EventType} eventType The event to dispatch.
   * @param {Touch} touch The touch triggering this event.
   * @return {boolean|undefined} The value of enableDrag after dispatching
   *         the event.
   * @private
   */
  dispatchEvent_(eventType, touch) {
    // Determine which element was touched.  For mouse events, this is always
    // the event/touch target.  But for touch events, the target is always the
    // target of the touchstart (and it's unlikely we can change this
    // since the common implementation of touch dragging relies on it). Since
    // touch is our primary scenario (which we want to emulate with mouse),
    // we'll treat both cases the same and not depend on the target.
    /** @type {Element} */
    let touchedElement;
    if (eventType === TouchHandler.EventType.TOUCH_START) {
      assertInstanceof(touch.target, Element);
      touchedElement = touch.target;
    } else {
      touchedElement = assert(this.element_.ownerDocument.elementFromPoint(
          touch.clientX, touch.clientY));
    }

    return this.dispatchEventXY_(
        eventType, touchedElement, touch.clientX, touch.clientY);
  },

  /**
   * Dispatch a TouchHandler event to the element
   * @param {!TouchHandler.EventType} eventType The event to dispatch.
   * @param {!Element} touchedElement
   * @param {number} clientX The X location for the event.
   * @param {number} clientY The Y location for the event.
   * @return {boolean|undefined} The value of enableDrag after dispatching
   *         the event.
   * @private
   */
  dispatchEventXY_(eventType, touchedElement, clientX, clientY) {
    const isDrag =
        (eventType === TouchHandler.EventType.DRAG_START ||
         eventType === TouchHandler.EventType.DRAG_MOVE ||
         eventType === TouchHandler.EventType.DRAG_END);

    // Drag events don't bubble - we're really just dragging the element,
    // not affecting its parent at all.
    const bubbles = !isDrag;

    const event = new TouchHandler.Event(
        eventType, bubbles, clientX, clientY, touchedElement);

    // Set enableDrag when it can be overridden
    if (eventType === TouchHandler.EventType.TOUCH_START) {
      event.enableDrag = false;
    } else if (eventType === TouchHandler.EventType.DRAG_START) {
      event.enableDrag = true;
    }

    if (isDrag) {
      event.dragDeltaX = clientX - this.startTouchX_;
      event.dragDeltaY = clientY - this.startTouchY_;
    }

    this.element_.dispatchEvent(event);
    return event.enableDrag;
  },
};
