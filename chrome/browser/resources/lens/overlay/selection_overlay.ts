// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './object_layer.js';
import './text_layer.js';
import './region_selection.js';
import './post_selection_renderer.js';

import {EventTracker} from '//resources/js/event_tracker.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ObjectLayerElement} from './object_layer.js';
import type {PostSelectionRendererElement} from './post_selection_renderer.js';
import type {RegionSelectionElement} from './region_selection.js';
import {getTemplate} from './selection_overlay.html.js';
import {CursorType, DRAG_THRESHOLD, DragFeature, emptyGestureEvent, type GestureEvent, GestureState} from './selection_utils.js';
import type {TextLayerElement} from './text_layer.js';

const RESIZE_THRESHOLD = 8;

export interface CursorData {
  cursor: CursorType;
}

export interface SelectionOverlayElement {
  $: {
    backgroundImage: HTMLImageElement,
    cursor: HTMLElement,
    objectSelectionLayer: ObjectLayerElement,
    postSelectionRenderer: PostSelectionRendererElement,
    regionSelectionLayer: RegionSelectionElement,
    selectionOverlay: HTMLElement,
    textSelectionLayer: TextLayerElement,
  };
}

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

  static get properties() {
    return {
      isResized: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      screenshotDataUri: String,
      cursorImgUri: String,
      isPointerInside: Boolean,
      currentGesture: emptyGestureEvent(),
    };
  }

  private eventTracker_: EventTracker = new EventTracker();
  // The current gesture event. The coordinate values are only accurate if a
  // gesture has started.
  private currentGesture: GestureEvent = emptyGestureEvent();
  // The feature currently being dragged. Once a feature responds to a drag
  // event, no other feature will receive gesture events.
  private draggingRespondent = DragFeature.NONE;
  private resizeObserver: ResizeObserver = new ResizeObserver(() => {
    this.handleResize();
  });
  // We need to listen to resizes on the selectionElements separately, since
  // resizeObserver will trigger before the selectionElements have a chance to
  // resize.
  private selectionElementsResizeObserver: ResizeObserver =
      new ResizeObserver(() => {
        this.handleSelectionElementsResize();
      });
  private initialWidth: number = 0;
  private initialHeight: number = 0;
  // Whether the selection overlay is its initial size, or has changed size.
  private isResized: boolean;
  // The data URI of the current overlay screenshot.
  private screenshotDataUri: string;
  private cursorImgUri: string = 'lens.svg';
  private cursorOffsetX: number = 3;
  private cursorOffsetY: number = 6;
  private isPointerInside = false;

  override connectedCallback() {
    super.connectedCallback();
    this.resizeObserver.observe(this);
    this.selectionElementsResizeObserver.observe(this.$.selectionOverlay);
    this.eventTracker_.add(
        document, 'set-cursor', (e: CustomEvent<CursorData>) => {
          if (e.detail.cursor === CursorType.POINTER) {
            this.setCursorToPointer();
          } else if (e.detail.cursor === CursorType.CROSSHAIR) {
            this.setCursorToCrosshair();
          } else if (e.detail.cursor === CursorType.TEXT) {
            this.setCursorToText();
          } else {
            this.resetCursor();
          }
        });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.resizeObserver.unobserve(this);
    this.selectionElementsResizeObserver.unobserve(this.$.selectionOverlay);
    this.eventTracker_.removeAll();
  }

  override ready() {
    super.ready();
    this.addEventListener('pointerdown', this.onPointerDown.bind(this));
    this.addEventListener('pointermove', this.updateCursorPosition.bind(this));
  }

  private addDragListeners() {
    this.addEventListener('pointerup', this.onPointerUp);
    this.addEventListener('pointermove', this.onPointerMove);
    this.addEventListener('pointercancel', this.onPointerCancel);
  }

  private removeDragListeners() {
    this.removeEventListener('pointerup', this.onPointerUp);
    this.removeEventListener('pointermove', this.onPointerMove);
    this.removeEventListener('pointercancel', this.onPointerCancel);
  }

  private updateCursorPosition(event: PointerEvent) {
    const mouseX = event.clientX;
    const mouseY = event.clientY;

    this.$.cursor.style.transform = `translate3d(${
        mouseX + this.cursorOffsetX}px, ${mouseY + this.cursorOffsetY}px, 0)`;
  }

  private getHiddenCursorClass(isPointerInside: boolean, state: GestureState):
      string {
    // Always show when dragging, even if outside the selection overlay.
    if (!isPointerInside && state !== GestureState.DRAGGING) {
      return 'hidden';
    } else {
      return '';
    }
  }

  // Called on text hover and drag.
  private setCursorToText() {
    // Set body cursor style to handle dragging.
    document.body.style.cursor = 'text';
    this.cursorImgUri = 'text.svg';
    this.cursorOffsetX = 3;
    this.cursorOffsetY = 8;
  }

  // Called on region selection drag.
  private setCursorToCrosshair() {
    // Set body cursor style to handle dragging.
    document.body.style.cursor = 'crosshair';
    this.cursorOffsetX = 3;
    this.cursorOffsetY = 6;
    this.cursorImgUri = 'search.svg';
  }

  // Called on object hover.
  private setCursorToPointer() {
    // No dragging for objects, so no need to set body cursor style.
    this.cursorOffsetX = 4;
    this.cursorOffsetY = 8;
    this.cursorImgUri = 'search.svg';
  }

  private resetCursor() {
    document.body.style.cursor = 'unset';
    this.cursorImgUri = 'lens.svg';
    this.cursorOffsetX = 3;
    this.cursorOffsetY = 6;
  }

  private handlePointerEnter() {
    this.isPointerInside = true;
  }

  private handlePointerLeave() {
    this.isPointerInside = false;
  }

  private onPointerDown(event: PointerEvent) {
    if (this.shouldIgnoreEvent(event)) {
      return;
    }

    this.dispatchEvent(new CustomEvent(
        'selection-overlay-clicked', {bubbles: true, composed: true}));
    this.addDragListeners();

    this.currentGesture = {
      state: GestureState.STARTING,
      startX: event.clientX,
      startY: event.clientY,
      clientX: event.clientX,
      clientY: event.clientY,
    };

    if (this.$.textSelectionLayer.handleDownGesture(this.currentGesture)) {
      // Text is responding to this sequence of gestures.
      this.draggingRespondent = DragFeature.TEXT;
      this.$.postSelectionRenderer.clearSelection();
    } else if (this.$.postSelectionRenderer.handleDownGesture(
                   this.currentGesture)) {
      this.draggingRespondent = DragFeature.POST_SELECTION;
    }
  }

  private onPointerUp(event: PointerEvent) {
    this.updateGestureCoordinates(event);

    // Allow proper feature to respond to the tap/drag event.
    switch (this.currentGesture.state) {
      case GestureState.DRAGGING:
        // Drag has finished. Let the features respond to the end of a drag.
        if (this.draggingRespondent === DragFeature.MANUAL_REGION) {
          this.$.regionSelectionLayer.handleUpGesture(this.currentGesture);
        } else if (this.draggingRespondent === DragFeature.TEXT) {
          this.$.textSelectionLayer.handleUpGesture();
        } else if (this.draggingRespondent === DragFeature.POST_SELECTION) {
          this.$.postSelectionRenderer.handleUpGesture();
        }
        break;
      case GestureState.STARTING:
        // This gesture was a tap. Let the features respond to a tap.
        this.$.objectSelectionLayer.handleUpGesture(this.currentGesture);
        break;
      default:  // Other states are invalid and ignored.
        break;
    }

    // After features have responded to the event, reset the current drag state.
    this.currentGesture = emptyGestureEvent();
    this.draggingRespondent = DragFeature.NONE;
    this.removeDragListeners();
    this.resetCursor();
  }

  private onPointerMove(event: PointerEvent) {
    // If a gesture hasn't started, ignore the pointer movement.
    if (this.currentGesture.state === GestureState.NOT_STARTED) {
      return;
    }

    this.updateGestureCoordinates(event);

    if (this.isDragging()) {
      this.set('currentGesture.state', GestureState.DRAGGING);

      // Capture pointer events so gestures still work if the users pointer
      // leaves the selection overlay div. Pointer capture is implicitly
      // released after pointerup or pointercancel events.
      this.setPointerCapture(event.pointerId);

      if (this.draggingRespondent === DragFeature.TEXT) {
        this.setCursorToText();
        this.$.textSelectionLayer.handleDragGesture(this.currentGesture);
      } else if (this.draggingRespondent === DragFeature.POST_SELECTION) {
        this.$.postSelectionRenderer.handleDragGesture(this.currentGesture);
      } else {
        // Let the features respond to the current drag if no other feature
        // responded first.
        this.setCursorToCrosshair();
        this.$.postSelectionRenderer.clearSelection();
        this.draggingRespondent = DragFeature.MANUAL_REGION;
        this.$.regionSelectionLayer.handleDragGesture(this.currentGesture);
      }
    }
  }

  private onPointerCancel() {
    // Pointer cancelled, so cancel any pending gestures.
    this.$.textSelectionLayer.cancelGesture();
    this.$.regionSelectionLayer.cancelGesture();
    this.$.postSelectionRenderer.cancelGesture();

    this.currentGesture = emptyGestureEvent();
    this.draggingRespondent = DragFeature.NONE;
    this.removeDragListeners();
    this.resetCursor();
  }

  private handleResize() {
    const newRect = this.getBoundingClientRect();

    if (this.initialHeight === 0 || this.initialWidth === 0) {
      this.initialWidth = newRect.width;
      this.initialHeight = newRect.height;
    }
    // We allow a buffer threshold when determining if the page has been
    // resized so that subtle one pixel adjustments don't trigger an entire
    // page reflow.
    this.isResized =
        Math.abs(newRect.height - this.initialHeight) >= RESIZE_THRESHOLD ||
        Math.abs(newRect.width - this.initialWidth) >= RESIZE_THRESHOLD;
  }

  handleSelectionElementsResize() {
    const selectionOverlayBounds =
        this.$.selectionOverlay.getBoundingClientRect();
    this.$.regionSelectionLayer.setCanvasSizeTo(
        selectionOverlayBounds.width, selectionOverlayBounds.height);
    this.$.objectSelectionLayer.setCanvasSizeTo(
        selectionOverlayBounds.width, selectionOverlayBounds.height);
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
