// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility file for storing shared types and helper functions for
 * the different selection components.
 */

// The number of pixels the pointer needs to move before being considered a drag
export const DRAG_THRESHOLD = 5;

export enum DragFeature {
  NONE = 0,
  TEXT = 1,
  MANUAL_REGION = 2,
  POST_SELECTION = 3,
}

export enum GestureState {
  // No gesture is currently happening.
  NOT_STARTED = 0,
  // A gesture is starting, indicated by a pointerdown event.
  STARTING = 1,
  // A drag is currently happening, indicated by pointer moving far enough away
  // from the initial gesture position.
  DRAGGING = 2,
  // A drag is finished, indicated by a pointerup event.
  FINISHED = 3,
}

export enum CursorType {
  DEFAULT = 0,
  POINTER = 1,
  CROSSHAIR = 2,
  TEXT = 3,
}

// Specifies which feature is requesting to control the Shimmer. Features are
// ordered by priority, meaning requesters with higher enum values can take
// control from lower value requesters, but not vice versa. For example, if
// CURSOR is the requester, and a new focus region gets called for SEGMENTATION,
// the focus region request will be executed. But if CURSOR sends a focus region
// request while SEGMENTATION has control, the request will be ignored.
export enum ShimmerControlRequester {
  NONE = 0,
  CURSOR = 1,
  POST_SELECTION = 2,
  SEGMENTATION = 3,
  MANUAL_REGION = 4,
  TRANSLATE = 5,
}

// Region sent to OverlayShimmerCanvasElement to focus the shimmer on.
// The numbers should be normalized to the image dimensions, between 0 and 1.
export interface OverlayShimmerFocusedRegion {
  top: number;
  left: number;
  width: number;
  height: number;
  requester: ShimmerControlRequester;
}

// Request to unfocus a region and relinquish control from the given requester.
export interface OverlayShimmerUnfocusRegion {
  requester: ShimmerControlRequester;
}

export interface GestureEvent {
  // The state of this event.
  state: GestureState;
  // The x coordinate (pixel value) this gesture started at.
  startX: number;
  // The y coordinate (pixel value) this gesture started at.
  startY: number;
  // The x coordinate (pixel value) this gesture is currently at.
  clientX: number;
  // The y coordinate (pixel value) this gesture is currently at.
  clientY: number;
}

/** A simple interface representing a point. */
export interface Point {
  x: number;
  y: number;
}

// Returns an empty GestureEvent
export function emptyGestureEvent(): GestureEvent {
  return {
    state: GestureState.NOT_STARTED,
    startX: 0,
    startY: 0,
    clientX: 0,
    clientY: 0,
  };
}

/**
 * Helper function to dispatch event to focus the shimmer on a region. This
 * should be used instead of directly dispatching the event, so if
 * implementation changes, it can be easily changed across the codebase.
 */
export function focusShimmerOnRegion(
    dispatchEl: HTMLElement, top: number, left: number, width: number,
    height: number, requester: ShimmerControlRequester) {
  dispatchEl.dispatchEvent(
      new CustomEvent<OverlayShimmerFocusedRegion>('focus-region', {
        bubbles: true,
        composed: true,
        detail: {
          top,
          left,
          width,
          height,
          requester,
        },
      }));
}

/**
 * Helper function to dispatch event to unfocus the shimmer. This should be used
 * instead of directly dispatching the event, so if implementation changes, it
 * can be easily changed across the codebase.
 */
export function unfocusShimmer(
    dispatchEl: HTMLElement, requester: ShimmerControlRequester) {
  dispatchEl.dispatchEvent(
      new CustomEvent<OverlayShimmerUnfocusRegion>('unfocus-region', {
        bubbles: true,
        composed: true,
        detail: {requester},
      }));
}

// Converts the clientX and clientY to be relative to the given parent bounds
// instead of the viewport. If the event is out of the parent bounds, returns
// the closest point to those bounds.
export function getRelativeCoordinate(
    coord: Point, parentBounds: DOMRect): Point {
  return {
    x: Math.max(0, Math.min(coord.x, parentBounds.right) - parentBounds.left),
    y: Math.max(0, Math.min(coord.y, parentBounds.bottom) - parentBounds.top),
  };
}
