// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assert, assertInstanceof} from 'chrome://resources/js/assert.m.js';
// clang-format on

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
/* #export */ let DragPosition;

/**
 * Type of an ongoing drag.
 * @enum {number}
 * @private
 */
const DragType = {
  NONE: 0,
  CURSOR: 1,
  KEYBOARD: 2,
};

/** @polymerBehavior */
/* #export */ const DragBehavior = {
  properties: {
    /** Whether or not drag is enabled (e.g. not mirrored). */
    dragEnabled: Boolean,

    /**
     * Whether or not to allow keyboard dragging.  If set to false, all
     * keystrokes will be ignored by this element.
     * @type {boolean}
     */
    keyboardDragEnabled: {
      type: Boolean,
      value: false,
    },

    /**
     * The number of pixels to drag on each keypress.
     * @type {number}
     */
    keyboardDragStepSize: {
      type: Number,
      value: 20,
    },
  },

  /**
   * The type of the currently ongoing drag.  If a keyboard drag is ongoing and
   * the user initiates a cursor drag, the keyboard drag should end before the
   * cursor drag starts.  If a cursor drag is onging, keyboard dragging should
   * be ignored.
   * @private {DragType}
   */
  dragType_: DragType.NONE,

  /**
   * The id of the element being dragged, or empty if not dragging.
   * @protected {string}
   */
  dragId: '',

  /** @private {HTMLDivElement|Element|undefined} */
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

  /** @private {?function(!Event)} */
  keyDownListener_: null,

  /**
   * @param {boolean} enabled
   * @param {(HTMLDivElement|Element)=} opt_container
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

    this.keyDownListener_ = this.onKeyDown_.bind(this);
    container.addEventListener('keydown', this.keyDownListener_);

    this.endDragListener_ = this.endCursorDrag_.bind(this);
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
    this.keyDownListener_ = null;
    container.removeEventListener('keydown', this.keyDownListener_);
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
    return this.startCursorDrag_(target, {x: e.pageX, y: e.pageY});
  },

  /**
   * @param {!Event} e The mouse move event.
   * @return {boolean}
   * @private
   */
  onMouseMove_(e) {
    e.preventDefault();
    return this.processCursorDrag_(e, {x: e.pageX, y: e.pageY});
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
    return this.startCursorDrag_(target, this.lastTouchLocation_);
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
    return this.processCursorDrag_(e, touchLocation);
  },

  /**
   * @param {!Event} e The key down event.
   * @return {boolean} false if event is consumed; true otherwise.
   * @private
   */
  onKeyDown_(e) {
    // Ignore keystrokes if keyboard dragging is disabled.
    if (this.keyboardDragEnabled === false) {
      return true;
    }

    // Ignore keystrokes if the event target is not draggable.
    if (!e.target.getAttribute('draggable')) {
      return true;
    }

    // Keyboard drags should not interrupt cursor drags.
    if (this.dragType_ === DragType.CURSOR) {
      return true;
    }

    let delta;
    switch (e.key) {
      case 'ArrowUp':
        delta = {x: 0, y: -this.keyboardDragStepSize};
        break;
      case 'ArrowDown':
        delta = {x: 0, y: this.keyboardDragStepSize};
        break;
      case 'ArrowLeft':
        delta = {x: -this.keyboardDragStepSize, y: 0};
        break;
      case 'ArrowRight':
        delta = {x: this.keyboardDragStepSize, y: 0};
        break;
      case 'Enter':
        e.preventDefault();
        this.endKeyboardDrag_();
        return false;
      default:
        return true;
    }

    e.preventDefault();

    if (this.dragType_ === DragType.NONE) {
      // Start drag
      const target = assertInstanceof(e.target, HTMLElement);
      this.startKeyboardDrag_(target);
    }

    this.dragOffset_.x += delta.x;
    this.dragOffset_.y += delta.y;

    this.processKeyboardDrag_(this.dragOffset_);

    return false;
  },

  /**
   * @param {!HTMLElement} target
   * @param {!DragPosition} eventLocation
   * @return {boolean}
   * @private
   */
  startCursorDrag_(target, eventLocation) {
    assert(this.dragEnabled);
    if (this.dragType_ === DragType.KEYBOARD) {
      this.endKeyboardDrag_();
    }
    this.dragId = target.id;
    this.dragStartLocation_ = eventLocation;
    this.dragType_ = DragType.CURSOR;
    return false;
  },

  /**
   * @return {boolean}
   * @private
   */
  endCursorDrag_() {
    assert(this.dragEnabled);
    if (this.dragType_ === DragType.CURSOR && this.callback_) {
      this.callback_(this.dragId, null);
    }
    this.cleanupDrag_();
    return false;
  },

  /**
   * @param {!Event} e The event which triggers this drag.
   * @param {!DragPosition} eventLocation The location of the event.
   * @return {boolean}
   * @private
   */
  processCursorDrag_(e, eventLocation) {
    assert(this.dragEnabled);
    if (this.dragType_ !== DragType.CURSOR) {
      return true;
    }
    this.executeCallback_(eventLocation);
    return false;
  },

  /**
   * @param {!HTMLElement} target
   * @private
   */
  startKeyboardDrag_(target) {
    assert(this.dragEnabled);
    if (this.dragType_ === DragType.CURSOR) {
      this.endCursorDrag_();
    }
    this.dragId = target.id;
    this.dragStartLocation_ = {x: 0, y: 0};
    this.dragOffset_ = {x: 0, y: 0};
    this.dragType_ = DragType.KEYBOARD;
  },

  /** @private */
  endKeyboardDrag_() {
    assert(this.dragEnabled);
    if (this.dragType_ === DragType.KEYBOARD && this.callback_) {
      this.callback_(this.dragId, null);
    }
    this.cleanupDrag_();
  },

  /**
   * @param {!DragPosition} dragPosition
   * @return {boolean}
   * @private
   */
  processKeyboardDrag_(dragPosition) {
    assert(this.dragEnabled);
    if (this.dragType_ !== DragType.KEYBOARD) {
      return true;
    }
    this.executeCallback_(dragPosition);
    return false;
  },

  /**
   * Cleans up state for all currently ongoing drags.
   * @private
   */
  cleanupDrag_() {
    this.dragId = '';
    this.dragStartLocation_ = {x: 0, y: 0};
    this.lastTouchLocation_ = null;
    this.dragType_ = DragType.NONE;
  },

  /**
   * @param {!DragPosition} dragPosition
   * @private
   */
  executeCallback_(dragPosition) {
    if (this.callback_) {
      const delta = {
        x: dragPosition.x - this.dragStartLocation_.x,
        y: dragPosition.y - this.dragStartLocation_.y,
      };
      this.callback_(this.dragId, delta);
    }
  },
};
