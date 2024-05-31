// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert, assertInstanceof} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeat} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getFallbackTheme, skColorToRgbaWithCustomAlpha} from './color_utils.js';
import {type CursorTooltipData, CursorTooltipType} from './cursor_tooltip.js';
import {CenterRotatedBox_CoordinateType} from './geometry.mojom-webui.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import type {LensPageCallbackRouter, OverlayTheme} from './lens.mojom-webui.js';
import {recordLensOverlayInteraction, UserAction} from './metrics_utils.js';
import {getTemplate} from './object_layer.html.js';
import type {OverlayObject} from './overlay_object.mojom-webui.js';
import {Polygon_CoordinateType} from './polygon.mojom-webui.js';
import type {PostSelectionBoundingBox} from './post_selection_renderer.js';
import type {CursorData} from './selection_overlay.js';
import {CursorType, focusShimmerOnRegion, type GestureEvent, ShimmerControlRequester, unfocusShimmer} from './selection_utils.js';
import {toPercent} from './values_converter.js';

// The percent of the selection layer width and height the object needs to take
// up to be considered full page.
const FULLSCREEN_OBJECT_THRESHOLD_PERCENT = 0.95;

// Returns true if the object has a valid bounding box and is renderable by the
// ObjectLayer.
function isObjectRenderable(object: OverlayObject): boolean {
  // For an object to be renderable, it must have a bounding box with normalized
  // coordinates.
  // TODO(b/330183480): Add rendering for IMAGE CoordinateType
  const objectBoundingBox = object.geometry?.boundingBox;
  if (!objectBoundingBox) {
    return false;
  }

  // Filter out objects covering the entire screen.
  if (objectBoundingBox.box.width >= FULLSCREEN_OBJECT_THRESHOLD_PERCENT &&
      objectBoundingBox.box.height >= FULLSCREEN_OBJECT_THRESHOLD_PERCENT) {
    return false;
  }

  // TODO(b/334940363): CoordinateType is being incorrectly set to
  // kUnspecified instead of kNormalized. Once this is fixed, change this
  // check back to objectBoundingBox.coordinateType ===
  // CenterRotatedBox_CoordinateType.kNormalized.
  return objectBoundingBox.coordinateType !==
      CenterRotatedBox_CoordinateType.kImage;
}

// Orders objects with larger areas before objects with smaller areas.
function compareArea(object1: OverlayObject, object2: OverlayObject): number {
  assert(object1.geometry);
  assert(object2.geometry);
  return object2.geometry.boundingBox.box.width *
      object2.geometry.boundingBox.box.height -
      object1.geometry.boundingBox.box.width *
      object1.geometry.boundingBox.box.height;
}

export interface ObjectLayerElement {
  $: {
    hiddenCanvas: HTMLCanvasElement,
    highlightImg: HTMLImageElement,
    objectsContainer: DomRepeat,
    objectSelectionCanvas: HTMLCanvasElement,
  };
}

/*
 * Element responsible for highlighting and selection text.
 */
export class ObjectLayerElement extends PolymerElement {
  static get is() {
    return 'lens-object-layer';
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
      renderedObjects: {
        type: Array,
        value: () => [],
      },
      debugMode: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableDebuggingMode'),
        reflectToAttribute: true,
      },
      preciseHighlight: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enablePreciseHighlight'),
        reflectToAttribute: true,
      },
      theme: {
        type: Object,
        value: getFallbackTheme,
      },
      screenshotDataUri: String,
    };
  }

  private canvasHeight: number;
  private canvasWidth: number;
  private canvasPhysicalHeight: number;
  private canvasPhysicalWidth: number;
  // Whether canvas is currently blank.
  private canvasIsBlank: boolean = true;
  private context: CanvasRenderingContext2D;
  private hiddenContext?: CanvasRenderingContext2D;
  // The data URI of the current overlay screenshot.
  private screenshotDataUri: string;
  // The objects rendered in this layer.
  private renderedObjects: OverlayObject[];
  // Whether precise object highlighting is enabled.
  private preciseHighlight: boolean;
  // The overlay theme.
  private theme: OverlayTheme;

  private readonly router: LensPageCallbackRouter =
      BrowserProxyImpl.getInstance().callbackRouter;
  private objectsReceivedListenerId: number|null = null;

  override ready() {
    super.ready();

    this.context = this.$.objectSelectionCanvas.getContext('2d')!;
    if (this.preciseHighlight) {
      this.hiddenContext = this.$.hiddenCanvas.getContext('2d')!;
    }
  }

  override connectedCallback() {
    super.connectedCallback();

    // Set up listener to receive objects from C++.
    this.objectsReceivedListenerId = this.router.objectsReceived.addListener(
        this.onObjectsReceived.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    // Remove listener to receive objects from C++.
    assert(this.objectsReceivedListenerId);
    this.router.removeListener(this.objectsReceivedListenerId);
    this.objectsReceivedListenerId = null;
  }

  handleUpGesture(event: GestureEvent): boolean {
    const objectIndex = this.objectIndexFromPoint(event.clientX, event.clientY);
    // Ignore if the click is not on an object.
    if (objectIndex === null) {
      return false;
    }

    const selectionRegion =
        this.renderedObjects[objectIndex].geometry!.boundingBox;

    // Issue the query
    BrowserProxyImpl.getInstance().handler.issueLensRequest(selectionRegion);

    // Send the region to be rendered on the page.
    this.dispatchEvent(new CustomEvent('render-post-selection', {
      bubbles: true,
      composed: true,
      detail: this.getPostSelectionRegion(selectionRegion),
    }));

    recordLensOverlayInteraction(UserAction.OBJECT_CLICK);

    return true;
  }

  private onSegmentationHovered(object: OverlayObject) {
    this.drawObject(this.context, object);
    this.focusShimmer(object);
    this.dispatchEvent(new CustomEvent<CursorData>('set-cursor', {
      bubbles: true,
      composed: true,
      detail: {cursor: CursorType.POINTER},
    }));
    this.dispatchEvent(
        new CustomEvent<CursorTooltipData>('set-cursor-tooltip', {
          bubbles: true,
          composed: true,
          detail: {tooltipType: CursorTooltipType.CLICK_SEARCH},
        }));
    this.dispatchEvent(new CustomEvent('darken-extra-scrim-opacity', {
      bubbles: true,
      composed: true,
    }));
  }

  private onSegmentationUnhovered() {
    this.clearCanvas(this.context);
    unfocusShimmer(this, ShimmerControlRequester.SEGMENTATION);
    this.dispatchEvent(new CustomEvent<CursorData>('set-cursor', {
      bubbles: true,
      composed: true,
      detail: {cursor: CursorType.DEFAULT},
    }));
    this.dispatchEvent(
        new CustomEvent<CursorTooltipData>('set-cursor-tooltip', {
          bubbles: true,
          composed: true,
          detail: {tooltipType: CursorTooltipType.REGION_SEARCH},
        }));
    this.dispatchEvent(new CustomEvent('lighten-extra-scrim-opacity', {
      bubbles: true,
      composed: true,
    }));
  }

  private handlePointerEnter(event: PointerEvent) {
    assertInstanceof(event.target, HTMLElement);
    const object = this.$.objectsContainer.itemForElement(event.target);
    if (this.preciseHighlight) {
      // Draw the object in the hidden canvas which is used to highlight the
      // object in the visible canvas when the pointer is inside the object.
      this.drawObject(this.hiddenContext!, object);
    } else {
      this.onSegmentationHovered(object);
    }
  }

  private handlePointerLeave() {
    if (this.preciseHighlight) {
      // Clear the hidden canvas and reset state.
      this.clearCanvas(this.hiddenContext!);
      this.canvasIsBlank = true;
    }
    this.onSegmentationUnhovered();
  }

  private handlePointerMove(event: MouseEvent) {
    if (!this.preciseHighlight) {
      return;
    }

    assertInstanceof(event.target, HTMLElement);
    if (this.hiddenContext!.isPointInPath(
            event.clientX * window.devicePixelRatio,
            event.clientY * window.devicePixelRatio)) {
      // Ensure the object is drawn only once.
      if (!this.canvasIsBlank) {
        return;
      }
      this.canvasIsBlank = false;
      const object = this.$.objectsContainer.itemForElement(event.target);
      this.onSegmentationHovered(object);

    } else {
      // Ensure the canvas is cleared only once.
      if (this.canvasIsBlank) {
        return;
      }
      this.canvasIsBlank = true;
      this.onSegmentationUnhovered();
    }
  }

  setCanvasSizeTo(width: number, height: number) {
    // Resetting the canvas width and height also clears the canvas.
    this.canvasWidth = width;
    this.canvasHeight = height;
    this.canvasPhysicalWidth = width * window.devicePixelRatio;
    this.canvasPhysicalHeight = height * window.devicePixelRatio;
    this.context.setTransform(
        window.devicePixelRatio, 0, 0, window.devicePixelRatio, 0, 0);
    if (this.preciseHighlight) {
      this.hiddenContext!.setTransform(
          window.devicePixelRatio, 0, 0, window.devicePixelRatio, 0, 0);
    }
  }

  private drawObject(context: CanvasRenderingContext2D, object: OverlayObject) {
    const polygons = object.geometry.segmentationPolygon;
    if (!polygons) {
      return;
    }

    context.beginPath();
    for (const polygon of polygons) {
      // TODO(b/330183480): Currently, we are assuming that polygon
      // coordinates are normalized. We should still implement
      // rendering in case this assumption is ever violated.
      if (polygon.coordinateType !== Polygon_CoordinateType.kNormalized) {
        continue;
      }

      const firstVertex = polygon.vertex[0];
      context.moveTo(
          firstVertex.x * this.canvasWidth, firstVertex.y * this.canvasHeight);
      for (const vertex of polygon.vertex.slice(1)) {
        context.lineTo(
            vertex.x * this.canvasWidth, vertex.y * this.canvasHeight);
      }
    }
    context.closePath();

    // Draw the highlight image clipped to the path.
    context.save();
    context.filter = 'none';
    context.clip();
    context.drawImage(
        this.$.highlightImg, 0, 0, this.canvasWidth, this.canvasHeight);
    context.restore();

    // Stroke the path on top of the image.
    context.lineCap = 'round';
    context.lineJoin = 'round';
    context.lineWidth = 6;
    context.filter = 'blur(8px)';
    // Fit a square around the bounding box to use for gradient coordinates.
    const objectBoundingBox = object.geometry.boundingBox;
    const longestEdge =
        Math.max(objectBoundingBox.box.width, objectBoundingBox.box.height);
    const left = (objectBoundingBox.box.x - longestEdge / 2) * this.canvasWidth;
    const top = (objectBoundingBox.box.y - longestEdge / 2) * this.canvasHeight;
    const right =
        (objectBoundingBox.box.x + longestEdge / 2) * this.canvasWidth;
    const bottom =
        (objectBoundingBox.box.y + longestEdge / 2) * this.canvasHeight;
    const gradient = context.createLinearGradient(
        left,
        top,
        right,
        bottom,
    );
    const segmentationColor =
        skColorToRgbaWithCustomAlpha(this.theme.selectionElement, 0.65);
    gradient.addColorStop(0, segmentationColor);
    gradient.addColorStop(1, segmentationColor);
    context.strokeStyle = gradient;
    context.stroke();
  }

  private clearCanvas(context: CanvasRenderingContext2D) {
    context.clearRect(0, 0, this.canvasWidth, this.canvasHeight);
  }

  private focusShimmer(object: OverlayObject) {
    const polygons = object.geometry.segmentationPolygon;
    if (!polygons) {
      return;
    }

    let leftMostPoint = 0;
    let rightMostPoint = 0;
    let topMostPoint = 0;
    let bottomMostPoint = 0;
    for (const polygon of polygons) {
      // TODO(b/330183480): Currently, we are assuming that polygon
      // coordinates are normalized. We should still implement
      // rendering in case this assumption is ever violated.
      if (polygon.coordinateType !== Polygon_CoordinateType.kNormalized) {
        continue;
      }

      const firstVertex = polygon.vertex[0];
      topMostPoint = firstVertex.y;
      bottomMostPoint = firstVertex.y;
      leftMostPoint = firstVertex.x;
      rightMostPoint = firstVertex.x;

      for (const vertex of polygon.vertex.slice(1)) {
        topMostPoint = Math.min(topMostPoint, vertex.y);
        bottomMostPoint = Math.max(bottomMostPoint, vertex.y);
        leftMostPoint = Math.min(leftMostPoint, vertex.x);
        rightMostPoint = Math.max(rightMostPoint, vertex.x);
      }
    }

    // Focus the shimmer on the segmentation object.
    focusShimmerOnRegion(
        this, topMostPoint, leftMostPoint, rightMostPoint - leftMostPoint,
        bottomMostPoint - topMostPoint, ShimmerControlRequester.SEGMENTATION);
  }

  private onObjectsReceived(objects: OverlayObject[]) {
    // Sort objects by descending bounding box areas so that smaller objects
    // are rendered over, and take priority over, larger objects.
    this.renderedObjects =
        objects.filter(o => isObjectRenderable(o)).sort(compareArea);
  }

  /** @return The CSS styles string for the given object. */
  private getObjectStyle(object: OverlayObject): string {
    // Objects without bounding boxes are filtered out, so guaranteed that
    // geometry is not null.
    const objectBoundingBox = object.geometry!.boundingBox;

    // TODO(b/330183480): Currently, we are assuming that object
    // coordinates are normalized. We should still implement
    // rendering in case this assumption is ever violated.
    // TODO(b/334940363): CoordinateType is being incorrectly set to
    // kUnspecified instead of kNormalized. Once this is fixed, change this
    // check back to objectBoundingBox.coordinateType !==
    // CenterRotatedBox_CoordinateType.kNormalized.
    if (objectBoundingBox.coordinateType ===
        CenterRotatedBox_CoordinateType.kImage) {
      return '';
    }

    // Put into an array instead of a long string to keep this code readable.
    const styles: string[] = [
      `width: ${toPercent(objectBoundingBox.box.width)}`,
      `height: ${toPercent(objectBoundingBox.box.height)}`,
      `top: ${
          toPercent(
              objectBoundingBox.box.y - (objectBoundingBox.box.height / 2))}`,
      `left: ${
          toPercent(
              objectBoundingBox.box.x - (objectBoundingBox.box.width / 2))}`,
      `transform: rotate(${objectBoundingBox.rotation}rad)`,
    ];
    return styles.join(';');
  }

  private getPostSelectionRegion(box: CenterRotatedBox):
      PostSelectionBoundingBox {
    const boundingBox = box.box;
    const top = boundingBox.y - (boundingBox.height / 2);
    const left = boundingBox.x - (boundingBox.width / 2);
    return {
      top,
      left,
      width: boundingBox.width,
      height: boundingBox.height,
    };
  }

  /**
   * @return Returns the index in renderedObjects of the object at the given
   *     point. Returns null if no object is at the given point.
   */
  private objectIndexFromPoint(x: number, y: number): number|null {
    // Find the top-most element at the clicked point that is an object.
    // elementFromPoint() may select non-object elements that have a higher
    // z-index.
    const elementsAtPoint = this.shadowRoot!.elementsFromPoint(x, y);
    for (const element of elementsAtPoint) {
      if (!(element instanceof HTMLElement)) {
        continue;
      }
      const index = this.$.objectsContainer.indexForElement(element);
      if (index !== null) {
        return index;
      }
    }
    return null;
  }

  // Testing method to get the objects on the page.
  getObjectNodesForTesting() {
    return this.shadowRoot!.querySelectorAll<HTMLElement>('.object');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-object-layer': ObjectLayerElement;
  }
}

customElements.define(ObjectLayerElement.is, ObjectLayerElement);
