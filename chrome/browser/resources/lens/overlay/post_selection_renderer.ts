// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {CenterRotatedBox_CoordinateType} from './geometry.mojom-webui.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import {focusShimmerOnRegion, ShimmerControlRequester, unfocusShimmer} from './overlay_shimmer.js';
import {getTemplate} from './post_selection_renderer.html.js';
import type {GestureEvent} from './selection_utils.js';
import {toPercent} from './values_converter.js';

// Bounding box send to PostSelectionRendererElement to render a bounding box.
// The numbers should be normalized to the image dimensions, between 0 and 1
export interface PostSelectionBoundingBox {
  top: number;
  left: number;
  width: number;
  height: number;
}

// The target currently being dragged on by the user.
enum DragTarget {
  NONE,
  WHOLE_BOX,
  TOP_LEFT,
  TOP_RIGHT,
  BOTTOM_RIGHT,
  BOTTOM_LEFT,
}

// The amount of pixels around the edge leave as a buffer so user can't drag too
// far. Exported for testing.
export const PERIMETER_SELECTION_PADDING_PX = 4;

// The value for the corner length when the animation finishes and the box is
// in a resting state. Exported for testing.
export const RESTING_CORNER_LENGTH_PX = 22;

function clamp(value: number, min: number, max: number): number {
  return Math.min(Math.max(value, min), max);
}

export interface PostSelectionRendererElement {
  $: {
    postSelection: HTMLElement,
  };
}

/*
 * Renders the users visual selection after one is made. This element is also
 * responsible for allowing the user to adjust their region to issue a new
 * Lens request.
 */
export class PostSelectionRendererElement extends PolymerElement {
  static get is() {
    return 'post-selection-renderer';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      top: Number,
      left: Number,
      height: Number,
      width: Number,
      screenshotDataUri: String,
      currentDragTarget: Number,
      cornerIds: Array,
      enableSelectionDragging: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  private eventTracker_: EventTracker = new EventTracker();
  // The bounds of the current selection
  private top: number = 0;
  private left: number = 0;
  private height: number = 0;
  private width: number = 0;
  // The data URI of the current overlay screenshot.
  private screenshotDataUri: string;
  // What is currently being dragged by the user.
  private currentDragTarget: DragTarget = DragTarget.NONE;
  // IDs used to generate the corner hitbox divs.
  private cornerIds: string[] =
      ['topLeft', 'topRight', 'bottomRight', 'bottomLeft'];
  // Listener IDs for events tracked from the browser.
  private listenerIds: number[];
  // The original bounds from the start of a drag.
  private originalBounds:
      PostSelectionBoundingBox = {left: 0, top: 0, width: 0, height: 0};
  private enableSelectionDragging: boolean =
      loadTimeData.getBoolean('enableSelectionDragging');

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        document, 'render-post-selection',
        (e: CustomEvent<PostSelectionBoundingBox>) => {
          this.onRenderPostSelection(e);
        });
    // Set up listener to listen to events from C++.
    this.listenerIds = [
      BrowserProxyImpl.getInstance()
          .callbackRouter.clearAllSelections.addListener(
              this.clearSelection.bind(this)),
      BrowserProxyImpl.getInstance()
          .callbackRouter.setPostRegionSelection.addListener(
              this.setSelection.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
    this.listenerIds.forEach(
        id => assert(
            BrowserProxyImpl.getInstance().callbackRouter.removeListener(id)));
    this.listenerIds = [];
  }

  clearSelection() {
    unfocusShimmer(this, ShimmerControlRequester.POST_SELECTION);
    this.height = 0;
    this.width = 0;
  }

  handleDownGesture(event: GestureEvent): boolean {
    this.currentDragTarget =
        this.dragTargetFromPoint(event.clientX, event.clientY);

    if (this.shouldHandleDownGesture()) {
      // User is dragging the post selection (if enabled) or resizing.
      this.originalBounds = {
        left: this.left,
        top: this.top,
        width: this.width,
        height: this.height,
      };
      return true;
    }
    return false;
  }

  handleDragGesture(event: GestureEvent) {
    const imageBounds = this.getBoundingClientRect();
    const normalizedX = (event.clientX - imageBounds.left) / imageBounds.width;
    const normalizedY = (event.clientY - imageBounds.top) / imageBounds.height;
    const normalizedStartX =
        (event.startX - imageBounds.left) / imageBounds.width;
    const normalizedStartY =
        (event.startY - imageBounds.top) / imageBounds.height;
    const normalizedMinBoxWidth = this.getMinBoxSize() / imageBounds.width;
    const normalizedMinBoxHeight = this.getMinBoxSize() / imageBounds.height;
    const normalizedPerimeterPaddingWidth =
        PERIMETER_SELECTION_PADDING_PX / imageBounds.width;
    const normalizedPerimeterPaddingHeight =
        PERIMETER_SELECTION_PADDING_PX / imageBounds.height;
    const minXValue = normalizedPerimeterPaddingWidth;
    const minYValue = normalizedPerimeterPaddingHeight;
    const maxXValue = 1 - normalizedPerimeterPaddingWidth;
    const maxYValue = 1 - normalizedPerimeterPaddingHeight;

    const currentLeft = this.left;
    const currentTop = this.top;
    const currentRight = this.left + this.width;
    const currentBottom = this.top + this.height;
    let newLeft;
    let newTop;
    let newRight;
    let newBottom;

    switch (this.currentDragTarget) {
      case DragTarget.TOP_LEFT:
        newLeft = Math.min(normalizedX, currentRight - normalizedMinBoxWidth);
        newTop = Math.min(normalizedY, currentBottom - normalizedMinBoxHeight);
        newRight = currentRight;
        newBottom = currentBottom;
        break;
      case DragTarget.TOP_RIGHT:
        newLeft = currentLeft;
        newTop = Math.min(normalizedY, currentBottom - normalizedMinBoxHeight);
        newRight = Math.max(normalizedX, currentLeft + normalizedMinBoxWidth);
        newBottom = currentBottom;
        break;
      case DragTarget.BOTTOM_RIGHT:
        newLeft = currentLeft;
        newTop = currentTop;
        newRight = Math.max(normalizedX, currentLeft + normalizedMinBoxWidth);
        newBottom = Math.max(normalizedY, currentTop + normalizedMinBoxHeight);
        break;
      case DragTarget.BOTTOM_LEFT:
        newLeft = Math.min(normalizedX, currentRight - normalizedMinBoxWidth);
        newTop = currentTop;
        newRight = currentRight;
        newBottom = Math.max(normalizedY, currentTop + normalizedMinBoxHeight);
        break;
      case DragTarget.WHOLE_BOX:
        // Only adjust if the drag will keep the image in bounds horizontally.
        const dragDeltaX = normalizedX - normalizedStartX;
        const originalRight =
            this.originalBounds.left + this.originalBounds.width;

        if (dragDeltaX >= 0) {
          // Drag is left to right
          if (dragDeltaX + originalRight <= maxXValue) {
            // Drag is in bounds
            newLeft = this.originalBounds.left + dragDeltaX;
            newRight = originalRight + dragDeltaX;
          } else {
            // Drag is out of bounds. Set the box as far right as possible.
            newLeft = maxXValue - this.width;
            newRight = maxXValue;
          }
        } else {
          // Drag is right to left
          if (dragDeltaX + this.originalBounds.left >= minXValue) {
            // Drag is in bounds
            newLeft = this.originalBounds.left + dragDeltaX;
            newRight = originalRight + dragDeltaX;
          } else {
            // Drag is out of bounds. Set the box as far left as possible.
            newLeft = minXValue;
            newRight = minXValue + this.width;
          }
        }

        // Only adjust if the drag will keep the image in bounds vertically.
        const dragDeltaY = normalizedY - normalizedStartY;
        const originalBottom =
            this.originalBounds.top + this.originalBounds.height;
        if (dragDeltaY >= 0) {
          // Drag is top to bottom
          if (dragDeltaY + originalBottom <= maxYValue) {
            // Drag is in bounds
            newTop = this.originalBounds.top + dragDeltaY;
            newBottom = originalBottom + dragDeltaY;
          } else {
            // Drag is out of bounds. Set the box as far down as possible.
            newTop = maxYValue - this.height;
            newBottom = maxYValue;
          }
        } else {
          // Drag is bottom to top
          if (dragDeltaY + this.originalBounds.top >= minYValue) {
            // Drag is in bounds
            newTop = this.originalBounds.top + dragDeltaY;
            newBottom = originalBottom + dragDeltaY;
          } else {
            // Drag is out of bounds. Set the box as far up as possible.
            newTop = minYValue;
            newBottom = minYValue + this.height;
          }
        }
        break;
      default:
        assertNotReached();
    }
    assert(newLeft);
    assert(newTop);
    assert(newRight);
    assert(newBottom);

    // Ensure the new region is within the image bounds.
    newLeft = clamp(newLeft, minXValue, maxXValue - normalizedMinBoxWidth);
    newTop = clamp(newTop, minYValue, maxYValue - normalizedMinBoxHeight);
    newRight = clamp(newRight, minXValue + normalizedMinBoxWidth, maxXValue);
    newBottom = clamp(newBottom, minYValue + normalizedMinBoxHeight, maxYValue);

    // Set the new dimensions.
    this.left = newLeft;
    this.top = newTop;
    this.width = newRight - newLeft;
    this.height = newBottom - newTop;

    this.rerender();
  }

  handleUpGesture() {
    if (this.shouldSendLensRequest()) {
      // Issue Lens request for new bounds
      BrowserProxyImpl.getInstance().handler.issueLensRequest(
          this.getNormalizedCenterRotatedBox());
    }

    this.originalBounds = {left: 0, top: 0, width: 0, height: 0};
    this.currentDragTarget = DragTarget.NONE;
  }

  cancelGesture() {
    this.originalBounds = {left: 0, top: 0, width: 0, height: 0};
    this.currentDragTarget = DragTarget.NONE;
  }

  enableSelectionDraggingForTesting() {
    this.enableSelectionDragging = true;
  }

  private setSelection(region: CenterRotatedBox) {
    const normalizedTop = region.box.y - (region.box.height / 2);
    const normalizedLeft = region.box.x - (region.box.width / 2);

    this.top = normalizedTop;
    this.left = normalizedLeft;
    this.height = region.box.height;
    this.width = region.box.width;
    this.originalBounds = {left: 0, top: 0, width: 0, height: 0};

    this.rerender();
    this.triggerNewBoxAnimation();
  }

  private onRenderPostSelection(e: CustomEvent<PostSelectionBoundingBox>) {
    this.top = e.detail.top;
    this.left = e.detail.left;
    this.height = e.detail.height;
    this.width = e.detail.width;

    this.rerender();
    this.triggerNewBoxAnimation();
  }

  private rerender() {
    // Set the CSS properties to reflect current bounds and force rerender.
    this.style.setProperty('--selection-width', toPercent(this.width));
    this.style.setProperty('--selection-height', toPercent(this.height));
    this.style.setProperty('--selection-top', toPercent(this.top));
    this.style.setProperty('--selection-left', toPercent(this.left));

    // Focus the shimmer on the new post selection region.
    focusShimmerOnRegion(
        this, this.top, this.left, this.width, this.height,
        ShimmerControlRequester.POST_SELECTION);
  }

  private triggerNewBoxAnimation() {
    const parentBoundingRect = this.getBoundingClientRect();
    this.animate(
        [
          {
            [`--post-selection-corner-horizontal-length`]:
                `${parentBoundingRect.width * this.width / 2}px`,
            [`--post-selection-corner-vertical-length`]:
                `${parentBoundingRect.height * this.height / 2}px`,
          },
          {
            [`--post-selection-corner-horizontal-length`]:
                `${RESTING_CORNER_LENGTH_PX}px`,
            [`--post-selection-corner-vertical-length`]:
                `${RESTING_CORNER_LENGTH_PX}px`,
          },
        ],
        {
          duration: 450,
          easing: 'cubic-bezier(0.2, 0.0, 0, 1.0)',
        });
  }

  // Returns if the current bounds should be sent to Lens.
  private shouldSendLensRequest() {
    return this.originalBounds.top !== this.top ||
        this.originalBounds.left !== this.left ||
        this.originalBounds.height !== this.height ||
        this.originalBounds.width !== this.width;
  }

  /**
   * @return Returns the drag target at the given point.
   */
  private dragTargetFromPoint(x: number, y: number): DragTarget {
    const topMostElements = this.shadowRoot!.elementsFromPoint(x, y);
    const topMostDraggableElement = topMostElements.find(el => {
      return (el instanceof HTMLElement) && el.classList.contains('draggable');
    });
    if (!topMostDraggableElement) {
      return DragTarget.NONE;
    }
    switch (topMostDraggableElement.id) {
      case 'topLeft':
        return DragTarget.TOP_LEFT;
      case 'topRight':
        return DragTarget.TOP_RIGHT;
      case 'bottomRight':
        return DragTarget.BOTTOM_RIGHT;
      case 'bottomLeft':
        return DragTarget.BOTTOM_LEFT;
      case 'selectionCorners':
        return DragTarget.WHOLE_BOX;
      default:
        // Did not click on a target we care about.
        break;
    }
    return DragTarget.NONE;
  }

  private shouldHandleDownGesture(): boolean {
    if (this.enableSelectionDragging) {
      return this.currentDragTarget !== DragTarget.NONE;
    }
    return this.currentDragTarget !== DragTarget.NONE &&
        this.currentDragTarget !== DragTarget.WHOLE_BOX;
  }

  // Converts the current region to a CenterRotatedBox
  private getNormalizedCenterRotatedBox(): CenterRotatedBox {
    return {
      box: {
        x: this.left + (this.width / 2),
        y: this.top + (this.height / 2),
        width: this.width,
        height: this.height,
      },
      rotation: 0,
      coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
    };
  }

  private getScrimStyleProperties() {
    // If there is no selection, set opacity to zero to trigger fade out
    // CSS transition.
    return !this.hasSelection() ? 'opacity: 0;' : '';
  }

  // Used in HTML template to know if there is currently a selection to render.
  private hasSelection(): boolean {
    return this.width > 0 && this.height > 0;
  }

  // Gets the minimum size the selected region can be. Public for testing.
  private getMinBoxSize(): number {
    return RESTING_CORNER_LENGTH_PX * 2;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'post-selection-renderer': PostSelectionRendererElement;
  }
}

customElements.define(
    PostSelectionRendererElement.is, PostSelectionRendererElement);

// Setup CSS Houdini API
CSS.paintWorklet.addModule('post_selection_paint_worklet.js');

// Variables controlling the rendered post selection
CSS.registerProperty({
  name: '--post-selection-corner-horizontal-length',
  syntax: '<length>',
  inherits: true,
  initialValue: '22px',
});
CSS.registerProperty({
  name: '--post-selection-corner-vertical-length',
  syntax: '<length>',
  inherits: true,
  initialValue: '22px',
});
CSS.registerProperty({
  name: '--post-selection-corner-width',
  syntax: '<length>',
  inherits: true,
  initialValue: '4px',
});
CSS.registerProperty({
  name: '--post-selection-corner-radius',
  syntax: '<length>',
  inherits: true,
  initialValue: '12px',
});
