// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {CenterRotatedBox_CoordinateType} from './geometry.mojom-webui.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import {getTemplate} from './post_selection_renderer.html.js';
import type {GestureEvent} from './selection_utils.js';

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

// Takes the value between 0-1 and returns a string in the from '__%';
// TODO(b/333620724): Move to a separate file and reuse across codebase.
function toPercent(value: number): string {
  return `${value * 100}%`;
}

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

  // The original bounds from the start of a drag.
  private originalBounds:
      PostSelectionBoundingBox = {left: 0, top: 0, width: 0, height: 0};
  // The px value of the size of the corner selection, extracted from the CSS
  // variable. Undefined if the value has yet to be extracted from the CSS.
  private cornerLength?: number;

  constructor() {
    super();

    // Setup CSS Houdini API
    CSS.paintWorklet.addModule('post_selection_paint_worklet.js');
  }

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        document, 'render-post-selection',
        (e: CustomEvent<PostSelectionBoundingBox>) => {
          this.onRenderPostSelection(e);
        });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  clearSelection() {
    this.height = 0;
    this.width = 0;
  }

  handleDownGesture(event: GestureEvent): boolean {
    this.currentDragTarget =
        this.dragTargetFromPoint(event.clientX, event.clientY);

    if (this.currentDragTarget !== DragTarget.NONE) {
      // User is dragging the post selection or resizing.
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

  private onRenderPostSelection(e: CustomEvent<PostSelectionBoundingBox>) {
    this.top = e.detail.top;
    this.left = e.detail.left;
    this.height = e.detail.height;
    this.width = e.detail.width;

    this.rerender();
  }

  private rerender() {
    // Set the CSS properties to reflect current bounds and force rerender.
    this.style.setProperty('--selection-width', toPercent(this.width));
    this.style.setProperty('--selection-height', toPercent(this.height));
    this.style.setProperty('--selection-top', toPercent(this.top));
    this.style.setProperty('--selection-left', toPercent(this.left));
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

  // Used in HTML template to know if there is currently a selection to render.
  private hasSelection(): boolean {
    return this.width > 0 && this.height > 0;
  }

  // Gets the minimum size the selected region can be. Public for testing.
  private getMinBoxSize(): number {
    if (!this.cornerLength) {
      // Cache the corner length to avoid multiple calls to computedStyleMap().
      this.cornerLength =
          parseInt(this.computedStyleMap().get('--corner-length')!.toString());
    }
    return this.cornerLength * 2;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'post-selection-renderer': PostSelectionRendererElement;
  }
}

customElements.define(
    PostSelectionRendererElement.is, PostSelectionRendererElement);
