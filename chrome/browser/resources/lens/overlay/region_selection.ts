// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Point} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {CenterRotatedBox_CoordinateType} from './geometry.mojom-webui.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import type {PostSelectionBoundingBox} from './post_selection_renderer.js';
import {getTemplate} from './region_selection.html.js';
import type {GestureEvent} from './selection_utils.js';

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
    };
  }

  private canvasHeight: number;
  private canvasWidth: number;
  private context: CanvasRenderingContext2D;

  override ready() {
    super.ready();

    this.context = this.$.regionSelectionCanvas.getContext('2d')!;
  }

  // Handles a drag gesture by drawing a bounded box on the canvas.
  handleDragGesture(event: GestureEvent) {
    this.clearCanvas();
    this.renderBoundingBox(event);
  }

  handleUpGesture(event: GestureEvent) {
    // Issue the Lens request
    BrowserProxyImpl.getInstance().handler.issueLensRequest(
        this.getNormalizedCenterRotatedBoxFromGesture(event));

    // Keep the region rendered on the page
    this.dispatchEvent(new CustomEvent('render-post-selection', {
      bubbles: true,
      composed: true,
      detail: this.getPostSelectionRegion(event),
    }));

    this.clearCanvas();
  }

  cancelGesture() {
    this.clearCanvas();
  }

  setCanvasSizeTo(width: number, height: number) {
    // Resetting the canvas width and height also clears the canvas.
    this.canvasWidth = width;
    this.canvasHeight = height;
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
    this.context.roundRect(
      left, top, width, height, radii);

    // Draw the highlight image clipped to the path.
    this.context.save();
    this.context.clip();
    this.context.drawImage(
        this.$.highlightImg, 0, 0, this.canvasWidth, this.canvasHeight);
    this.context.restore();

    // Stroke the path on top of the image.
    this.context.stroke();
  }

  /**
   * @returns a mojo CenterRotatedBox corresponding to the gesture provided,
   *          normalized to the selection overlay dimensions.
   */
  private getNormalizedCenterRotatedBoxFromGesture(gesture: GestureEvent):
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
