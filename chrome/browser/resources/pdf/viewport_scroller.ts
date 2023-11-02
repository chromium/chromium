// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Point} from './constants.js';

interface ScrollVelocity {
  x: number;
  y: number;
}

export interface ViewportInterface {
  position: Point;
  setPosition(point: Point): void;
}

// Scrolls the page in response to drag selection with the mouse.
export class ViewportScroller {
  private viewport_: ViewportInterface;
  private plugin_: HTMLElement;
  private window_: Window;
  private mousemoveCallback_: ((e: MouseEvent) => void)|null = null;
  private timerId_: number|null = null;
  private scrollVelocity_: ScrollVelocity|null = null;
  private lastFrameTime_: number = 0;

  /**
   * @param viewport The viewport info of the page.
   * @param plugin The PDF plugin element.
   * @param window The window containing the viewer.
   */
  constructor(
      viewport: ViewportInterface, plugin: HTMLElement, window: Window) {
    this.viewport_ = viewport;
    this.plugin_ = plugin;
    this.window_ = window;
  }

  /**
   * Start scrolling the page by |scrollVelocity_| every
   * |DRAG_TIMER_INTERVAL_MS_|.
   */
  private startDragScrollTimer_() {
    if (this.timerId_ !== null) {
      return;
    }

    this.timerId_ = this.window_.setInterval(
        this.dragScrollPage_.bind(this), DRAG_TIMER_INTERVAL_MS);
    this.lastFrameTime_ = Date.now();
  }

  /** Stops the drag scroll timer if it is active. */
  private stopDragScrollTimer_() {
    if (this.timerId_ === null) {
      return;
    }

    this.window_.clearInterval(this.timerId_);
    this.timerId_ = null;
    this.lastFrameTime_ = 0;
  }

  /** Scrolls the viewport by the current scroll velocity. */
  private dragScrollPage_() {
    const position = this.viewport_.position;
    const currentFrameTime = Date.now();
    const timeAdjustment =
        (currentFrameTime - this.lastFrameTime_) / DRAG_TIMER_INTERVAL_MS;
    position.y += (this.scrollVelocity_!.y * timeAdjustment);
    position.x += (this.scrollVelocity_!.x * timeAdjustment);
    this.viewport_.setPosition(position);
    this.lastFrameTime_ = currentFrameTime;
  }

  /**
   * Calculate the velocity to scroll while dragging using the distance of the
   * cursor outside the viewport.
   * @return Object with x and y direction scroll velocity.
   */
  private calculateVelocity_(event: MouseEvent): ScrollVelocity {
    const x =
        Math.min(
            Math.max(
                -event.offsetX, event.offsetX - this.plugin_.offsetWidth, 0),
            MAX_DRAG_SCROLL_DISTANCE) *
        Math.sign(event.offsetX);
    const y =
        Math.min(
            Math.max(
                -event.offsetY, event.offsetY - this.plugin_.offsetHeight, 0),
            MAX_DRAG_SCROLL_DISTANCE) *
        Math.sign(event.offsetY);
    return {x: x, y: y};
  }

  /**
   * Handles mousemove events. It updates the scroll velocity and starts and
   * stops timer based on scroll velocity.
   */
  private onMousemove_(event: MouseEvent) {
    this.scrollVelocity_ = this.calculateVelocity_(event);
    if (!this.scrollVelocity_!.x && !this.scrollVelocity_!.y) {
      this.stopDragScrollTimer_();
    } else if (!this.timerId_) {
      this.startDragScrollTimer_();
    }
  }

  /**
   * Sets whether to scroll the viewport when the mouse is outside the
   * viewport.
   * @param isSelecting Represents selection status.
   */
  setEnableScrolling(isSelecting: boolean) {
    if (isSelecting) {
      if (!this.mousemoveCallback_) {
        this.mousemoveCallback_ = this.onMousemove_.bind(this);
      }
      this.plugin_.addEventListener(
          'mousemove', this.mousemoveCallback_, false);
    } else {
      this.stopDragScrollTimer_();
      if (this.mousemoveCallback_) {
        this.plugin_.removeEventListener(
            'mousemove', this.mousemoveCallback_, false);
      }
    }
  }
}

/**
 * The period of time in milliseconds to wait between updating the viewport
 * position by the scroll velocity.
 */
const DRAG_TIMER_INTERVAL_MS: number = 100;

/**
 * The maximum drag scroll distance per DRAG_TIMER_INTERVAL in pixels.
 */
const MAX_DRAG_SCROLL_DISTANCE: number = 100;
