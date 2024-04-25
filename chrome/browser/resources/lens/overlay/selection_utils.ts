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
