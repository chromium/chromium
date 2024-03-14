// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './selection_overlay.html.js';
import {DRAG_THRESHOLD, DragFeature, emptyGestureEvent, type GestureEvent, GestureState} from './selection_utils.js';

/*
 * Element responsible for coordinating selections between the various selection
 * features. This includes:
 *   - Storing state needed to coordinate selections across features
 *   - Listening to mouse/tap events and delegating them to the correct features
 *   - Coordinating animations between the different features
 */
export class SelectionOverlayElement extends PolymerElement {
  static get is() {
    return 'lens-selection-overlay';
  }

  static get template() {
    return getTemplate();
  }

  // The current gesture event. The coordinate values are only accurate if a
  // gesture has started.
  private currentGesture: GestureEvent = emptyGestureEvent();
  // The feature currently being dragged. Once a feature responds to a drag
  // event, no other feature will receive gesture events.
  private draggingRespondent = DragFeature.NONE;

  override ready() {
    super.ready();
    this.addEventListener('pointerdown', this.onPointerDown.bind(this));
    this.addEventListener('pointerup', this.onPointerUp.bind(this));
    this.addEventListener('pointermove', this.onPointerMove.bind(this));
  }

  private onPointerDown(event: PointerEvent) {
    if (this.shouldIgnoreEvent(event)) {
      return;
    }

    this.currentGesture = {
      state: GestureState.STARTING,
      startX: event.clientX,
      startY: event.clientY,
      clientX: event.clientX,
      clientY: event.clientY,
    };
  }

  private onPointerUp(event: PointerEvent) {
    if (this.shouldIgnoreEvent(event)) {
      return;
    }

    this.updateGestureCoordinates(event);

    // Allow proper feature to respond to the tap/drag event.
    switch (this.currentGesture.state) {
      case GestureState.DRAGGING:
        // Drag has finished. Let the features respond to the end of a drag.
        break;
      case GestureState.STARTING:
        // This gesture was a tap. Let the features respond to a tap.
        break;
      default:  // Other states are invalid and ignored.
        break;
    }

    // After features have responded to the event, reset the current drag state.
    this.currentGesture = emptyGestureEvent();
  }

  private onPointerMove(event: PointerEvent) {
    // If a gesture hasn't started, ignore the pointer movement.
    if (this.currentGesture.state === GestureState.NOT_STARTED) {
      return;
    }

    this.updateGestureCoordinates(event);

    if (this.isDragging()) {
      this.currentGesture.state = GestureState.DRAGGING;

      // Let the features respond to the current drag.
    }
  }

  // Updates the currentGesture to correspond with the given PointerEvent.
  private updateGestureCoordinates(event: PointerEvent) {
    this.currentGesture.clientX = event.clientX;
    this.currentGesture.clientY = event.clientY;
  }

  // Returns if the given PointerEvent should be ignored.
  private shouldIgnoreEvent(event: PointerEvent) {
    // Ignore multi touch events and none left click events.
    return !event.isPrimary || event.button !== 0;
  }

  // Returns whether the current gesture event is a drag.
  private isDragging() {
    if (this.currentGesture.state === GestureState.DRAGGING) {
      return true;
    }

    // TODO(b/329514345): Revisit if pointer movement is enough of an indicator,
    // or if we also need a timelimit on how long a tap can last before starting
    // a drag.
    const xMovement =
        Math.abs(this.currentGesture.clientX - this.currentGesture.startX);
    const yMovement =
        Math.abs(this.currentGesture.clientY - this.currentGesture.startY);
    return xMovement > DRAG_THRESHOLD || yMovement > DRAG_THRESHOLD;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-selection-overlay': SelectionOverlayElement;
  }
}

customElements.define(SelectionOverlayElement.is, SelectionOverlayElement);
