// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getFallbackTheme, getShaderLayerColorHexes} from './color_utils.js';
import {CenterRotatedBox_CoordinateType} from './geometry.mojom-webui.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import type {OverlayTheme} from './lens.mojom-webui.js';
import {UserAction} from './lens.mojom-webui.js';
import {INVOCATION_SOURCE} from './lens_overlay_app.js';
import {recordLensOverlayInteraction} from './metrics_utils.js';
import type {PostSelectionBoundingBox} from './post_selection_renderer.js';
import {getTemplate} from './region_selection.html.js';
import {ScreenshotBitmapBrowserProxyImpl} from './screenshot_bitmap_browser_proxy.js';
import {renderScreenshot} from './screenshot_utils.js';
import {focusShimmerOnRegion, type GestureEvent, GestureState, getRelativeCoordinate, ShimmerControlRequester, unfocusShimmer} from './selection_utils.js';
import type {Point} from './selection_utils.js';

// A simple interface representing a rectangle with normalized values.
interface NormalizedRectangle {
  center: Point;
  top: number;
  left: number;
  width: number;
  height: number;
}

export interface RegionSelectionElement {
  $: {
    highlightImgCanvas: HTMLCanvasElement,
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
      shaderLayerColorHexes: {
        type: Array,
        computed: 'computeShaderLayerColorHexes_(theme)',
      },
      theme: {
        type: Object,
        value: getFallbackTheme,
      },
      selectionOverlayRect: Object,
    };
  }

  private canvasHeight: number;
  private canvasWidth: number;
  private canvasPhysicalHeight: number;
  private canvasPhysicalWidth: number;
  private context: CanvasRenderingContext2D;
  // The data URI of the current overlay screenshot.
  private screenshotDataUri: string;
  // The overlay theme.
  private theme: OverlayTheme;
  // The bounds of the parent element. This is updated by the parent to avoid
  // this class needing to call getBoundingClientRect()
  private selectionOverlayRect: DOMRect;
  // Shader hex colors.
  private shaderLayerColorHexes: string[];
  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();

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

  override connectedCallback() {
    super.connectedCallback();

    ScreenshotBitmapBrowserProxyImpl.getInstance().fetchScreenshot(
        (screenshot: ImageBitmap) => {
          renderScreenshot(this.$.highlightImgCanvas, screenshot);
        });
  }

  private computeShaderLayerColorHexes_() {
    return getShaderLayerColorHexes(this.theme);
  }

  // Handles a drag gesture by drawing a bounded box on the canvas.
  handleGestureDrag(event: GestureEvent) {
    this.clearCanvas();
    this.renderBoundingBox(event);
  }

  handleGestureEnd(event: GestureEvent): boolean {
    // Issue the Lens request.
    const isClick = event.state === GestureState.STARTING;
    this.browserProxy.handler.issueLensRegionRequest(
        this.getNormalizedCenterRotatedBoxFromGesture(event), isClick);

    // Relinquish control from the shimmer.
    unfocusShimmer(this, ShimmerControlRequester.MANUAL_REGION);

    // Keep the region rendered on the page
    this.dispatchEvent(new CustomEvent('render-post-selection', {
      bubbles: true,
      composed: true,
      detail: this.getPostSelectionRegion(event),
    }));

    // Check for selectable text
    this.dispatchEvent(new CustomEvent('detect-text-in-region', {
      bubbles: true,
      composed: true,
      detail: this.getNormalizedCenterRotatedBoxFromGesture(event),
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

  private renderBoundingBox(event: GestureEvent, idealCornerRadius = 24) {
    const parentRect = this.selectionOverlayRect;

    // Get the drag event coordinates relative to the canvas
    const relativeDragStart =
        getRelativeCoordinate({x: event.startX, y: event.startY}, parentRect);
    const relativeDragEnd =
        getRelativeCoordinate({x: event.clientX, y: event.clientY}, parentRect);

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
    gradient.addColorStop(0, this.shaderLayerColorHexes[0]);
    gradient.addColorStop(0.5, this.shaderLayerColorHexes[1]);
    gradient.addColorStop(1, this.shaderLayerColorHexes[2]);
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
        this.$.highlightImgCanvas, 0, 0, this.canvasWidth, this.canvasHeight);
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
    const normalizedRect = this.getNormalizedRectangleFromTap(gesture);
    return {
      box: {
        x: normalizedRect.center.x,
        y: normalizedRect.center.y,
        width: normalizedRect.width,
        height: normalizedRect.height,
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
    const parentRect = this.selectionOverlayRect;
    // Get coordinates relative to the region selection bounds
    const relativeDragStart = getRelativeCoordinate(
        {x: gesture.startX, y: gesture.startY}, parentRect);
    const relativeDragEnd = getRelativeCoordinate(
        {x: gesture.clientX, y: gesture.clientY}, parentRect);

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
      recordLensOverlayInteraction(
          INVOCATION_SOURCE, UserAction.kTapRegionSelection);
      return this.getPostSelectionRegionFromTap(gesture);
    }

    recordLensOverlayInteraction(
        INVOCATION_SOURCE, UserAction.kRegionSelection);
    return this.getPostSelectionRegionFromDrag(gesture);
  }

  private getPostSelectionRegionFromTap(gesture: GestureEvent):
      PostSelectionBoundingBox {
    const normalizedRect = this.getNormalizedRectangleFromTap(gesture);
    return {
      top: normalizedRect.top,
      left: normalizedRect.left,
      width: normalizedRect.width,
      height: normalizedRect.height,
    };
  }

  private getPostSelectionRegionFromDrag(gesture: GestureEvent):
      PostSelectionBoundingBox {
    const parentRect = this.selectionOverlayRect;

    // Get coordinates relative to the region selection bounds
    const relativeDragStart = getRelativeCoordinate(
        {x: gesture.startX, y: gesture.startY}, parentRect);
    const relativeDragEnd = getRelativeCoordinate(
        {x: gesture.clientX, y: gesture.clientY}, parentRect);

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

  private getNormalizedRectangleFromTap(gesture: GestureEvent):
      NormalizedRectangle {
    const parentRect = this.selectionOverlayRect;
    // The size of the canvas relative to the size of the viewport.
    const scaleFactor = Math.min(
        parentRect.height / window.innerHeight,
        parentRect.width / window.innerWidth);
    const tapRegionWidth =
        loadTimeData.getInteger('tapRegionWidth') * scaleFactor;
    const tapRegionHeight =
        loadTimeData.getInteger('tapRegionWidth') * scaleFactor;

    // If the parent is smaller than our defined tap region, we should just send
    // the entire screenshot.
    if (parentRect.width < tapRegionWidth ||
        parentRect.height < tapRegionHeight) {
      return {
        top: 0,
        left: 0,
        center: {x: 0.5, y: 0.5},
        width: 1,
        height: 1,
      };
    }

    const normalizedWidth = tapRegionWidth / parentRect.width;
    const normalizedHeight = tapRegionHeight / parentRect.height;

    // Get the ideal left and top by making sure the region is always within
    // the bounds of the parent rect.
    const idealCenterPoint = getRelativeCoordinate(
        {x: gesture.clientX, y: gesture.clientY}, parentRect);
    let centerX = Math.max(idealCenterPoint.x, tapRegionWidth / 2);
    let centerY = Math.max(idealCenterPoint.y, tapRegionHeight / 2);
    centerX = Math.min(centerX, parentRect.width - tapRegionWidth / 2);
    centerY = Math.min(centerY, parentRect.height - tapRegionHeight / 2);

    const top = centerY - (tapRegionHeight / 2);
    const left = centerX - (tapRegionWidth / 2);

    const normalizedTop = top / parentRect.height;
    const normalizedLeft = left / parentRect.width;
    const normalizedCenterX = centerX / parentRect.width;
    const normalizedCenterY = centerY / parentRect.height;
    return {
      top: normalizedTop,
      left: normalizedLeft,
      center: {x: normalizedCenterX, y: normalizedCenterY},
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
