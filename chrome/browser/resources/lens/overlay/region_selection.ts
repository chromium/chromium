// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import type {Point} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {CenterRotatedBox_CoordinateType} from './geometry.mojom-webui.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import {getTemplate} from './region_selection.html.js';
import type {GestureEvent} from './selection_utils.js';

export interface RegionSelectionElement {
  $: {regionSelectionCanvas: HTMLCanvasElement};
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
  private parentResizeObserver: ResizeObserver = new ResizeObserver(() => {
    this.setCanvasSizeToParent();
  });

  override connectedCallback() {
    super.connectedCallback();

    assert(this.parentElement);
    this.parentResizeObserver.observe(this.parentElement);
  }

  override disconnectedCallback() {
    assert(this.parentElement);
    this.parentResizeObserver.unobserve(this.parentElement);
  }

  override ready() {
    super.ready();

    this.context = this.$.regionSelectionCanvas.getContext('2d')!;
  }

  // Handles a drag gesture by drawing a bounded box on the canvas.
  handleDragGesture(event: GestureEvent) {
    this.clearCanvas();
    this.renderDashedBoundingBox(event);
  }

  handleUpGesture(event: GestureEvent) {
    BrowserProxyImpl.getInstance().handler.issueLensRequest(
        this.getNormalizedCenterRotatedBoxFromGesture(event));
  }

  cancelGesture() {
    this.clearCanvas();
  }

  private setCanvasSizeToParent() {
    assert(this.parentElement);
    // Resetting the canvas width and height also clears the canvas.
    this.canvasWidth = this.parentElement.offsetWidth;
    this.canvasHeight = this.parentElement.offsetHeight;
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

  private renderDashedBoundingBox(event: GestureEvent, idealCornerRadius = 12) {
    const dashLength = 6;
    const gapLength = 5;

    // Get the drag event coordinates relative to the canvas
    const relativeDragStart =
        this.getRelativeCoordinate({x: event.startX, y: event.startY});
    const relativeDragEnd =
        this.getRelativeCoordinate({x: event.clientX, y: event.clientY});

    // Get the dimensions of the box from the gesture event points.
    const width = Math.abs(relativeDragEnd.x - relativeDragStart.x);
    const height = Math.abs(relativeDragEnd.y - relativeDragStart.y);

    // Define the points for the bounding box for readability.
    const topLeftX = Math.min(relativeDragStart.x, relativeDragEnd.x);
    const topLeftY = Math.min(relativeDragStart.y, relativeDragEnd.y);

    this.context.setLineDash([dashLength, gapLength]);
    this.context.lineWidth = 2;
    this.context.fillStyle = 'rgba(255, 255, 255, 0.3)';
    this.context.strokeStyle = 'white';

    // Draw the path for the region bounding box.
    this.context.beginPath();
    this.context.roundRect(
        topLeftX, topLeftY, width, height, idealCornerRadius);
    this.context.stroke();
    this.context.fill();
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
}

declare global {
  interface HTMLElementTagNameMap {
    'region-selection': RegionSelectionElement;
  }
}

customElements.define(RegionSelectionElement.is, RegionSelectionElement);
