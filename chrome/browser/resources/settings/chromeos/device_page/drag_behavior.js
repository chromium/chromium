// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for handling dragging elements in a container.
 *     Draggable elements must have the 'draggable' attribute set.
 */

/**
 * @typedef {{
 *   x: number,
 *   y: number
 * }}
 */
let DragPosition;

/** @polymerBehavior */
const DragBehavior = {
  properties: {
    /** Whether or not drag is enabled (e.g. not mirrored). */
    dragEnabled: Boolean,
  },

  /**
   * The id of the element being dragged, or empty if not dragging.
   * @protected {string}
   */
  dragId: '',

  /** @private {!HTMLDivElement|undefined} */
  container_: undefined,

  /** @private {?function(string, ?DragPosition):void} */
  callback_: null,

  /** @private {!DragPosition} */
  dragStartLocation_: {x: 0, y: 0},

  /**
   * Used to ignore unnecessary drag events.
   * @private {?DragPosition}
   */
  lastTouchLocation_: null,

  /** @private {?function(!Event)} */
  mouseDownListener_: null,

  /** @private {?function(!Event)} */
  mouseMoveListener_: null,

  /** @private {?function(!Event)} */
  touchStartListener_: null,

  /** @private {?function(!Event)} */
  touchMoveListener_: null,

  /** @private {?function(!Event)} */
  endDragListener_: null,

  /**
   * @param {boolean} enabled
   * @param {!HTMLDivElement=} opt_container
   * @param {!function(string, ?DragPosition):void=} opt_callback
   */
  initializeDrag(enabled, opt_container, opt_callback) {
    this.dragEnabled = enabled;
    if (!enabled) {
      this.removeListeners_();
      return;
    }

    if (opt_container !== undefined) {
      this.container_ = opt_container;
    }

    this.addListeners_();

    if (opt_callback !== undefined) {
      this.callback_ = opt_callback;
    }
  },

  /** @private */
  addListeners_() {
    const container = this.container_;
    if (!container || this.mouseDownListener_) {
      return;
    }
    this.mouseDownListener_ = this.onMouseDown_.bind(this);
    container.addEventListener('mousedown', this.mouseDownListener_);

    this.mouseMoveListener_ = this.onMouseMove_.bind(this);
    container.addEventListener('mousemove', this.mouseMoveListener_);

    this.touchStartListener_ = this.onTouchStart_.bind(this);
    container.addEventListener('touchstart', this.touchStartListener_);

    this.touchMoveListener_ = this.onTouchMove_.bind(this);
    container.addEventListener('touchmove', this.touchMoveListener_);

    this.endDragListener_ = this.endDrag_.bind(this);
    window.addEventListener('mouseup', this.endDragListener_);
    container.addEventListener('touchend', this.endDragListener_);
  },

  /** @private */
  removeListeners_() {
    const container = this.container_;
    if (!container || !this.mouseDownListener_) {
      return;
    }
    container.removeEventListener('mousedown', this.mouseDownListener_);
    this.mouseDownListener_ = null;
    container.removeEventListener('mousemove', this.mouseMoveListener_);
    this.mouseMoveListener_ = null;
    container.removeEventListener('touchstart', this.touchStartListener_);
    this.touchStartListener_ = null;
    container.removeEventListener('touchmove', this.touchMoveListener_);
    this.touchMoveListener_ = null;
    container.removeEventListener('touchend', this.endDragListener_);
    if (this.endDragListener_) {
      window.removeEventListener('mouseup', this.endDragListener_);
    }
    this.endDragListener_ = null;
  },

  /**
   * @param {!Event} e The mouse down event.
   * @return {boolean}
   * @private
   */
  onMouseDown_(e) {
    if (e.button !== 0 || !e.target.getAttribute('draggable')) {
      return true;
    }
    e.preventDefault();
    const target = assertInstanceof(e.target, HTMLElement);
    return this.startDrag_(target, {x: e.pageX, y: e.pageY});
  },

  /**
   * @param {!Event} e The mouse move event.
   * @return {boolean}
   * @private
   */
  onMouseMove_(e) {
    e.preventDefault();
    return this.processDrag_(e, {x: e.pageX, y: e.pageY});
  },

  /**
   * @param {!Event} e The touch start event.
   * @return {boolean}
   * @private
   */
  onTouchStart_(e) {
    if (e.touches.length !== 1) {
      return false;
    }

    e.preventDefault();
    const touch = e.touches[0];
    this.lastTouchLocation_ = {x: touch.pageX, y: touch.pageY};
    const target = assertInstanceof(e.target, HTMLElement);
    return this.startDrag_(target, this.lastTouchLocation_);
  },

  /**
   * @param {!Event} e The touch move event.
   * @return {boolean}
   * @private
   */
  onTouchMove_(e) {
    if (e.touches.length !== 1) {
      return true;
    }

    const touchLocation = {x: e.touches[0].pageX, y: e.touches[0].pageY};
    // Touch move events can happen even if the touch location doesn't change
    // and on small unintentional finger movements. Ignore these small changes.
    if (this.lastTouchLocation_) {
      const IGNORABLE_TOUCH_MOVE_PX = 1;
      const xDiff = Math.abs(touchLocation.x - this.lastTouchLocation_.x);
      const yDiff = Math.abs(touchLocation.y - this.lastTouchLocation_.y);
      if (xDiff <= IGNORABLE_TOUCH_MOVE_PX &&
          yDiff <= IGNORABLE_TOUCH_MOVE_PX) {
        return true;
      }
    }
    this.lastTouchLocation_ = touchLocation;
    e.preventDefault();
    return this.processDrag_(e, touchLocation);
  },

  /**
   * @param {!HTMLElement} target
   * @param {!DragPosition} eventLocation
   * @return {boolean}
   * @private
   */
  startDrag_(target, eventLocation) {
    assert(this.dragEnabled);
    this.dragId = target.id;
    this.dragStartLocation_ = eventLocation;
    return false;
  },

  /**
   * @param {!Event} e
   * @return {boolean}
   * @private
   */
  endDrag_(e) {
    assert(this.dragEnabled);
    if (this.dragId && this.callback_) {
      this.callback_(this.dragId, null);
    }
    this.dragId = '';
    this.lastTouchLocation_ = null;
    return false;
  },

  /**
   * @param {!Event} e The event which triggers this drag.
   * @param {DragPosition} eventLocation The location of the event.
   * @return {boolean}
   * @private
   */
  processDrag_(e, eventLocation) {
    assert(this.dragEnabled);
    if (!this.dragId) {
      return true;
    }
    if (this.callback_) {
      const delta = {
        x: eventLocation.x - this.dragStartLocation_.x,
        y: eventLocation.y - this.dragStartLocation_.y,
      };
      this.callback_(this.dragId, delta);
    }
    return false;
  },
};
