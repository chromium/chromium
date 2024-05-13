// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {Point} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {CenterRotatedBox_CoordinateType} from './geometry.mojom-webui.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import {focusShimmerOnRegion, ShimmerControlRequester, unfocusShimmer} from './overlay_shimmer.js';
import type {PostSelectionBoundingBox} from './post_selection_renderer.js';
import {getTemplate} from './region_selection.html.js';
import {type GestureEvent, GestureState} from './selection_utils.js';

export interface RegionSelectionElement {
  $: {
    highlightImg: HTMLImageElement,
    regionSelectionCanvas: HTMLCanvasElement,
  };
}

/*
 * Element responsible for rendering the region being selected by the user. It
 * does not render any post-selection state.
 */
export class RegionSelectionElement extends PolymerElement {
  static get is() {
    return 'region-selection';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      canvasHeight: Number,
      canvasWidth: Number,
      canvasPhysicalHeight: Number,
      canvasPhysicalWidth: Number,
      screenshotDataUri: String,
    };
  }

  private canvasHeight: number;
  private canvasWidth: number;
  private canvasPhysicalHeight: number;
  private canvasPhysicalWidth: number;
  private context: CanvasRenderingContext2D;
  // The data URI of the current overlay screenshot.
  private screenshotDataUri: string;
  // The tap region dimensions are the height and width that the region should
  // have when the user taps instead of drag.
  private readonly tapRegionHeight: number =
      loadTimeData.getInteger('tapRegionHeight');
  private readonly tapRegionWidth: number =
      loadTimeData.getInteger('tapRegionWidth');

  override ready() {
    super.ready();

    this.context = this.$.regionSelectionCanvas.getContext('2d')!;
  }

  // Handles a drag gesture by drawing a bounded box on the canvas.
  handleDragGesture(event: GestureEvent) {
    this.clearCanvas();
    this.renderBoundingBox(event);
  }

  handleUpGesture(event: GestureEvent): boolean {
    // Issue the Lens request
    BrowserProxyImpl.getInstance().handler.issueLensRequest(
        this.getNormalizedCenterRotatedBoxFromGesture(event));

    // Relinquish control from the shimmer.
    unfocusShimmer(this, ShimmerControlRequester.MANUAL_REGION);

    // Keep the region rendered on the page
    this.dispatchEvent(new CustomEvent('render-post-selection', {
      bubbles: true,
      composed: true,
      detail: this.getPostSelectionRegion(event),
    }));

    this.clearCanvas();
    return true;
  }

  cancelGesture() {
    this.clearCanvas();
  }

  setCanvasSizeTo(width: number, height: number) {
    // Resetting the canvas width and height also clears the canvas.
    this.canvasWidth = width;
    this.canvasHeight = height;
    this.canvasPhysicalWidth = width * window.devicePixelRatio;
    this.canvasPhysicalHeight = height * window.devicePixelRatio;
    this.context.setTransform(
        window.devicePixelRatio, 0, 0, window.devicePixelRatio, 0, 0);
  }

  private clearCanvas() {
    this.context.clearRect(0, 0, this.canvasWidth, this.canvasHeight);
  }

  // Converts the clientX and clientY to be relative to the Region Selection
  // Canvas bounds instead of the viewport. If the event is out of the region
  // selection canvas bounds, returns the closest point on the overlay.
  private getRelativeCoordinate(coord: Point): Point {
    const boundingRect = this.getBoundingClientRect();

    return {
      x: Math.max(0, Math.min(coord.x, boundingRect.right) - boundingRect.left),
      y: Math.max(0, Math.min(coord.y, boundingRect.bottom) - boundingRect.top),
    };
  }

  private renderBoundingBox(event: GestureEvent, idealCornerRadius = 24) {
    // Get the drag event coordinates relative to the canvas
    const relativeDragStart =
        this.getRelativeCoordinate({x: event.startX, y: event.startY});
    const relativeDragEnd =
        this.getRelativeCoordinate({x: event.clientX, y: event.clientY});

    // Get the dimensions of the box from the gesture event points.
    const width = Math.abs(relativeDragEnd.x - relativeDragStart.x);
    const height = Math.abs(relativeDragEnd.y - relativeDragStart.y);

    // Define the points for the bounding box for readability.
    const left = Math.min(relativeDragStart.x, relativeDragEnd.x);
    const top = Math.min(relativeDragStart.y, relativeDragEnd.y);
    const right = Math.max(relativeDragStart.x, relativeDragEnd.x);
    const bottom = Math.max(relativeDragStart.y, relativeDragEnd.y);

    // Get the vertical and horizontal directions of the drag.
    const isDraggingDown = relativeDragEnd.y > relativeDragStart.y;
    const isDraggingRight = relativeDragEnd.x > relativeDragStart.x;

    this.context.lineWidth = 3;
    const gradient = this.context.createLinearGradient(
        left,
        bottom,
        right,
        top,
    );
    gradient.addColorStop(0, '#C5E9EB');
    gradient.addColorStop(0.5, '#FFB2BD');
    gradient.addColorStop(1, '#028488');
    this.context.strokeStyle = gradient;

    // Draw the path for the region bounding box.
    this.context.beginPath();
    // The corner corresponding to the user's cursor should have 0 radius.
    const radii = [
      isDraggingDown || isDraggingRight ? idealCornerRadius : 0,
      isDraggingDown || !isDraggingRight ? idealCornerRadius : 0,
      !isDraggingDown || !isDraggingRight ? idealCornerRadius : 0,
      !isDraggingDown || isDraggingRight ? idealCornerRadius : 0,
    ];
    this.context.roundRect(left, top, width, height, radii);

    // Draw the highlight image clipped to the path.
    this.context.save();
    this.context.clip();
    this.context.drawImage(
        this.$.highlightImg, 0, 0, this.canvasWidth, this.canvasHeight);
    this.context.restore();

    // Stroke the path on top of the image.
    this.context.stroke();

    // Focus the shimmer on the new manually selected region.
    focusShimmerOnRegion(
        this, top / this.canvasHeight, left / this.canvasWidth,
        width / this.canvasWidth, height / this.canvasHeight,
        ShimmerControlRequester.MANUAL_REGION);
  }

  private getNormalizedCenterRotatedBoxFromGesture(gesture: GestureEvent):
      CenterRotatedBox {
    if (gesture.state === GestureState.STARTING) {
      return this.getNormalizedCenterRotatedBoxFromTap(gesture);
    }

    return this.getNormalizedCenterRotatedBoxFromDrag(gesture);
  }

  private getNormalizedCenterRotatedBoxFromTap(gesture: GestureEvent):
      CenterRotatedBox {
    const parentRect = this.getBoundingClientRect();
    // If the parent is smaller than our defined tap region, we should just send
    // the entire screenshot.
    if (parentRect.width < this.tapRegionWidth ||
        parentRect.height < this.tapRegionHeight) {
      return {
        box: {
          x: 0.5,
          y: 0.5,
          width: 1,
          height: 1,
        },
        rotation: 0,
        coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
      };
    }


    const normalizedWidth = this.tapRegionWidth / parentRect.width;
    const normalizedHeight = this.tapRegionHeight / parentRect.height;

    // Get the ideal left and top by making sure the region is always within
    // the bounds of the parent rect.
    const idealCenterPoint =
        this.getRelativeCoordinate({x: gesture.clientX, y: gesture.clientY});
    let centerX = Math.max(idealCenterPoint.x, this.tapRegionWidth / 2);
    let centerY = Math.max(idealCenterPoint.y, this.tapRegionHeight / 2);
    centerX = Math.min(centerX, parentRect.width - this.tapRegionWidth / 2);
    centerY = Math.min(centerY, parentRect.height - this.tapRegionHeight / 2);

    const normalizedCenterX = centerX / parentRect.width;
    const normalizedCenterY = centerY / parentRect.height;

    return {
      box: {
        x: normalizedCenterX,
        y: normalizedCenterY,
        width: normalizedWidth,
        height: normalizedHeight,
      },
      rotation: 0,
      coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
    };
  }

  /**
   * @returns a mojo CenterRotatedBox corresponding to the gesture provided,
   *          normalized to the selection overlay dimensions. The gesture is
   *          expected to be a drag.
   */
  private getNormalizedCenterRotatedBoxFromDrag(gesture: GestureEvent):
      CenterRotatedBox {
    const parentRect = this.getBoundingClientRect();

    // Get coordinates relative to the region selection bounds
    const relativeDragStart =
        this.getRelativeCoordinate({x: gesture.startX, y: gesture.startY});
    const relativeDragEnd =
        this.getRelativeCoordinate({x: gesture.clientX, y: gesture.clientY});

    const normalizedWidth =
        Math.abs(relativeDragEnd.x - relativeDragStart.x) / parentRect.width;
    const normalizedHeight =
        Math.abs(relativeDragEnd.y - relativeDragStart.y) / parentRect.height;
    const centerX = (relativeDragEnd.x + relativeDragStart.x) / 2;
    const centerY = (relativeDragEnd.y + relativeDragStart.y) / 2;
    const normalizedCenterX = centerX / parentRect.width;
    const normalizedCenterY = centerY / parentRect.height;
    return {
      box: {
        x: normalizedCenterX,
        y: normalizedCenterY,
        width: normalizedWidth,
        height: normalizedHeight,
      },
      rotation: 0,
      coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
    };
  }

  private getPostSelectionRegion(gesture: GestureEvent):
      PostSelectionBoundingBox {
    if (gesture.state === GestureState.STARTING) {
      return this.getPostSelectionRegionFromTap(gesture);
    }

    return this.getPostSelectionRegionFromDrag(gesture);
  }

  private getPostSelectionRegionFromTap(gesture: GestureEvent):
      PostSelectionBoundingBox {
    const parentRect = this.getBoundingClientRect();
    // If the parent is smaller than our defined tap region, we should just send
    // the entire screenshot.
    if (parentRect.width < this.tapRegionWidth ||
        parentRect.height < this.tapRegionHeight) {
      return {
        top: 0,
        left: 0,
        width: 1,
        height: 1,
      };
    }

    const normalizedWidth = this.tapRegionWidth / parentRect.width;
    const normalizedHeight = this.tapRegionHeight / parentRect.height;

    // Get the ideal left and top by making sure the region is always within
    // the bounds of the parent rect.
    const idealCenterPoint =
        this.getRelativeCoordinate({x: gesture.clientX, y: gesture.clientY});
    let top = Math.max(idealCenterPoint.y - this.tapRegionHeight / 2, 0);
    let left = Math.max(idealCenterPoint.x - this.tapRegionWidth / 2, 0);
    top = Math.min(top, parentRect.height - this.tapRegionHeight);
    left = Math.min(left, parentRect.width - this.tapRegionWidth);

    const normalizedTop = top / parentRect.height;
    const normalizedLeft = left / parentRect.width;

    return {
      top: normalizedTop,
      left: normalizedLeft,
      width: normalizedWidth,
      height: normalizedHeight,
    };
  }

  private getPostSelectionRegionFromDrag(gesture: GestureEvent):
      PostSelectionBoundingBox {
    const parentRect = this.getBoundingClientRect();

    // Get coordinates relative to the region selection bounds
    const relativeDragStart =
        this.getRelativeCoordinate({x: gesture.startX, y: gesture.startY});
    const relativeDragEnd =
        this.getRelativeCoordinate({x: gesture.clientX, y: gesture.clientY});

    const normalizedWidth =
        Math.abs(relativeDragEnd.x - relativeDragStart.x) / parentRect.width;
    const normalizedHeight =
        Math.abs(relativeDragEnd.y - relativeDragStart.y) / parentRect.height;
    const normalizedTop =
        Math.min(relativeDragEnd.y, relativeDragStart.y) / parentRect.height;
    const normalizedLeft =
        Math.min(relativeDragEnd.x, relativeDragStart.x) / parentRect.width;

    return {
      top: normalizedTop,
      left: normalizedLeft,
      width: normalizedWidth,
      height: normalizedHeight,
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'region-selection': RegionSelectionElement;
  }
}

customElements.define(RegionSelectionElement.is, RegionSelectionElement);
