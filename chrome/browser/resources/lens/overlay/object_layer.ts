// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert, assertInstanceof} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeat} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getFallbackTheme, skColorToRgbaWithCustomAlpha} from './color_utils.js';
import {type CursorTooltipData, CursorTooltipType} from './cursor_tooltip.js';
import {CenterRotatedBox_CoordinateType} from './geometry.mojom-webui.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import type {LensPageCallbackRouter, OverlayTheme} from './lens.mojom-webui.js';
import {UserAction} from './lens.mojom-webui.js';
import {INVOCATION_SOURCE} from './lens_overlay_app.js';
import {recordLensOverlayInteraction} from './metrics_utils.js';
import {getTemplate} from './object_layer.html.js';
import type {OverlayObject} from './overlay_object.mojom-webui.js';
import {Polygon_CoordinateType} from './polygon.mojom-webui.js';
import type {Vertex} from './polygon.mojom-webui.js';
import type {PostSelectionBoundingBox} from './post_selection_renderer.js';
import {ScreenshotBitmapBrowserProxyImpl} from './screenshot_bitmap_browser_proxy.js';
import {renderScreenshot} from './screenshot_utils.js';
import type {CursorData} from './selection_overlay.js';
import {CursorType, focusShimmerOnRegion, type GestureEvent, ShimmerControlRequester, unfocusShimmer} from './selection_utils.js';
import {toPercent} from './values_converter.js';

// The percent of the selection layer width and height the object needs to take
// up to be considered full page.
const FULLSCREEN_OBJECT_THRESHOLD_PERCENT = 0.95;
// The transition duration for the fade out animation into the cursor state.
const CURSOR_FADE_OUT_TRANSITION_DURATION = 150;

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

// Returns true if the object has a segmentation mask.
function hasSegmentationMask(object: OverlayObject): boolean {
  assert(object.geometry);
  return object.geometry.segmentationPolygon.length > 0;
}

// Comparator to order objects with larger areas before objects with smaller
// areas.
function compareArea(object1: OverlayObject, object2: OverlayObject): number {
  assert(object1.geometry);
  assert(object2.geometry);
  return object2.geometry.boundingBox.box.width *
      object2.geometry.boundingBox.box.height -
      object1.geometry.boundingBox.box.width *
      object1.geometry.boundingBox.box.height;
}

// Comparator to order objects with segmentation masks with larger areas before
// objects with segmentation masks with smaller areas.
function compareSegmentationMaskArea(
    object1: OverlayObject, object2: OverlayObject): number {
  assert(object1.geometry);
  assert(object2.geometry);
  return getSegmentationMaskArea(object2) - getSegmentationMaskArea(object1);
}

// Calculates the area of the segmentation mask of the object using the
// shoelace formula. Uses signed area so that counter-clockwise polygons
// (holes) are subtracted.
function getSegmentationMaskArea(object: OverlayObject): number {
  let area = 0;
  for (const polygon of object.geometry.segmentationPolygon) {
    const vertices = polygon.vertex;
    for (let i = 0; i < vertices.length; i++) {
      if (i < vertices.length - 1) {
        area += vertices[i].x * vertices[i + 1].y -
            vertices[i + 1].x * vertices[i].y;
      } else {
        area += vertices[i].x * vertices[0].y - vertices[0].x * vertices[i].y;
      }
    }
  }
  return 0.5 * area;
}

// Returns a clip path value for the object corresponding to its segmentation
// mask. If there is no segmentation mask, returns the value 'none'.
function toCssClipPath(object: OverlayObject): string {
  const polygons = object.geometry.segmentationPolygon;
  if (!polygons) {
    return 'none';
  }

  const points: string[] = [];
  for (const polygon of polygons) {
    // TODO(b/330183480): Currently, we are assuming that polygon
    // coordinates are normalized. We should still implement
    // rendering in case this assumption is ever violated.
    if (polygon.coordinateType !== Polygon_CoordinateType.kNormalized) {
      continue;
    }

    for (const vertex of polygon.vertex) {
      points.push(toCssPolygonVertex(object, vertex));
    }
    // Add first vertex again to close the path.
    points.push(toCssPolygonVertex(object, polygon.vertex[0]));
  }
  if (points.length === 0) {
    return 'none';
  }
  return 'polygon(evenodd, ' + points.join(', ') + ')';
}

// Converts the vertex to a string containing a pair of length-percentage values
// relative to the object bounding box, to be used in the CSS polygon()
// function.
function toCssPolygonVertex(object: OverlayObject, vertex: Vertex): string {
  const objectBoundingBox = object.geometry!.boundingBox;
  return toPercent(
             0.5 +
             (vertex.x - objectBoundingBox.box.x) /
                 objectBoundingBox.box.width) +
      ' ' +
      toPercent(
             0.5 +
             (vertex.y - objectBoundingBox.box.y) /
                 objectBoundingBox.box.height);
}

export interface ObjectLayerElement {
  $: {
    highlightImgCanvas: HTMLCanvasElement,
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
      theme: {
        type: Object,
        value: getFallbackTheme,
      },
    };
  }

  private eventTracker_: EventTracker = new EventTracker();
  private canvasHeight: number;
  private canvasWidth: number;
  private canvasPhysicalHeight: number;
  private canvasPhysicalWidth: number;
  private context: CanvasRenderingContext2D;
  // The objects rendered in this layer.
  private renderedObjects: OverlayObject[];
  // The last post selection made. Updated by events from the post selection
  // layer.
  private lastPostSelection: PostSelectionBoundingBox|null = null;
  // The overlay theme.
  private theme: OverlayTheme;
  private fadeOutAnimations: Animation[] = [];
  private fadeOutTimeoutIds: number[] = [];
  private postSelectionComparisonThreshold: number =
      loadTimeData.getValue('postSelectionComparisonThreshold');

  private readonly router: LensPageCallbackRouter =
      BrowserProxyImpl.getInstance().callbackRouter;
  private objectsReceivedListenerId: number|null = null;
  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.context = this.$.objectSelectionCanvas.getContext('2d')!;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        document, 'post-selection-updated',
        (e: CustomEvent<PostSelectionBoundingBox>) => {
          this.lastPostSelection = e.detail;
        });
    // Set up listener to receive objects from C++.
    this.objectsReceivedListenerId = this.router.objectsReceived.addListener(
        this.onObjectsReceived.bind(this));

    ScreenshotBitmapBrowserProxyImpl.getInstance().fetchScreenshot(
        (screenshot: ImageBitmap) => {
          renderScreenshot(this.$.highlightImgCanvas, screenshot);
        });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    // Remove listener to receive objects from C++.
    assert(this.objectsReceivedListenerId);
    this.router.removeListener(this.objectsReceivedListenerId);
    this.objectsReceivedListenerId = null;
  }

  handleGestureEnd(event: GestureEvent): boolean {
    const objectIndex = this.objectIndexFromPoint(event.clientX, event.clientY);
    // Ignore if the click is not on an object.
    if (objectIndex === null) {
      return false;
    }

    const object = this.renderedObjects[objectIndex];
    const selectionRegion = object.geometry!.boundingBox;

    // Issue the query.
    this.browserProxy.handler.issueLensObjectRequest(
        selectionRegion, hasSegmentationMask(object));

    // Send the region to be rendered on the page.
    this.dispatchEvent(new CustomEvent('render-post-selection', {
      bubbles: true,
      composed: true,
      detail: this.getPostSelectionRegion(selectionRegion),
    }));

    // Since the selection is made and rendering is being done by the post
    // selection layer, act as the cursor left so the segmentation is no longer
    // highlighted.
    this.handlePointerLeave();

    recordLensOverlayInteraction(INVOCATION_SOURCE, UserAction.kObjectClick);

    return true;
  }

  private handlePointerEnter(event: PointerEvent) {
    assertInstanceof(event.target, HTMLElement);

    // Only continue if we have an object that has a segmentation mask and is
    // not already selected.
    const object = this.$.objectsContainer.itemForElement(event.target);
    if (object === null || !hasSegmentationMask(object) ||
        this.isRegionAlreadySelected(
            this.getPostSelectionRegion(object.geometry!.boundingBox))) {
      return;
    }

    this.clearAndCancelAnimation();
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
    this.style.cursor = 'pointer';
  }

  private isRegionAlreadySelected(boundingBox: PostSelectionBoundingBox):
      boolean {
    if (this.lastPostSelection === null) {
      return false;
    }
    return Math.abs(boundingBox.top - this.lastPostSelection.top) <=
        this.postSelectionComparisonThreshold &&
        Math.abs(boundingBox.left - this.lastPostSelection.left) <=
        this.postSelectionComparisonThreshold &&
        Math.abs(boundingBox.width - this.lastPostSelection.width) <=
        this.postSelectionComparisonThreshold &&
        Math.abs(boundingBox.height - this.lastPostSelection.height) <=
        this.postSelectionComparisonThreshold;
  }

  private handlePointerLeave() {
    this.fadeOutAnimations.push(
        this.$.objectSelectionCanvas.animate({opacity: 0}, {
          duration: CURSOR_FADE_OUT_TRANSITION_DURATION,
          fill: 'forwards',
        }));
    this.fadeOutTimeoutIds.push(setTimeout(() => {
      this.clearCanvas(this.context);
    }, CURSOR_FADE_OUT_TRANSITION_DURATION));
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
    this.style.cursor = 'unset';
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

  private drawObject(context: CanvasRenderingContext2D, object: OverlayObject) {
    const polygons = object.geometry.segmentationPolygon;
    if (!polygons) {
      return;
    }

    context.beginPath();
    const cornerRadius =
        loadTimeData.getInteger('segmentationMaskCornerRadius');
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
      // Draw the segmentation mask, rounding each corner by the configured
      // radius.
      for (let i = 1; i < polygon.vertex.length; i++) {
        const currentVertex = polygon.vertex[i];
        const previousVertex = polygon.vertex[i - 1];

        // Calculate the distance between the current and previous vertices.
        const dx = currentVertex.x - previousVertex.x;
        const dy = currentVertex.y - previousVertex.y;
        const distance = Math.sqrt(dx * dx + dy * dy);

        // The control point distance should be the desired relative corner
        // radius or the radius of the arc between the two points. Whichever is
        // smaller.
        const controlPointDistance =
            Math.min(distance / 2, cornerRadius / this.canvasWidth);

        // Use linear interpolation to find the control points.
        const controlPoint1x =
            previousVertex.x + (dx * controlPointDistance) / distance;
        const controlPoint1y =
            previousVertex.y + (dy * controlPointDistance) / distance;
        const controlPoint2x =
            currentVertex.x - (dx * controlPointDistance) / distance;
        const controlPoint2y =
            currentVertex.y - (dy * controlPointDistance) / distance;

        context.lineTo(
            controlPoint1x * this.canvasWidth,
            controlPoint1y * this.canvasHeight);
        context.arcTo(
            controlPoint1x * this.canvasWidth,
            controlPoint1y * this.canvasHeight,
            controlPoint2x * this.canvasWidth,
            controlPoint2y * this.canvasHeight, cornerRadius);
      }
    }
    context.closePath();

    // Draw the highlight image clipped to the path.
    context.save();
    context.filter = 'none';
    context.clip();
    context.drawImage(
        this.$.highlightImgCanvas, 0, 0, this.canvasWidth, this.canvasHeight);
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

    // Create a new blank path so isPointInPath returns false.
    context.beginPath();
    context.closePath();
  }

  private focusShimmer(object: OverlayObject) {
    const polygons = object.geometry.segmentationPolygon;
    if (!polygons) {
      return;
    }

    const firstVertex = polygons[0].vertex[0];
    let topMostPoint = firstVertex.y;
    let bottomMostPoint = firstVertex.y;
    let leftMostPoint = firstVertex.x;
    let rightMostPoint = firstVertex.x;

    for (const polygon of polygons) {
      // TODO(b/330183480): Currently, we are assuming that polygon
      // coordinates are normalized. We should still implement
      // rendering in case this assumption is ever violated.
      if (polygon.coordinateType !== Polygon_CoordinateType.kNormalized) {
        continue;
      }

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
    // Sort objects with segmentation masks after objects without
    // segmentation masks. Then sort by descending segmentation mask or
    // bounding box area so that smaller objects are rendered over, and take
    // priority over, larger objects.
    const renderableObjects = objects.filter(o => isObjectRenderable(o));
    const objectsWithMask: OverlayObject[] = [];
    const objectsWithoutMask: OverlayObject[] = [];
    for (const object of renderableObjects) {
      if (hasSegmentationMask(object)) {
        objectsWithMask.push(object);
      } else {
        objectsWithoutMask.push(object);
      }
    }
    objectsWithMask.sort(compareSegmentationMaskArea);
    objectsWithoutMask.sort(compareArea);
    this.renderedObjects = objectsWithoutMask.concat(objectsWithMask);
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
      `clip-path: ${toCssClipPath(object)}`,
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

  // Clears any animation state currently present on the canvas.
  private clearAndCancelAnimation() {
    // Clear and cancel any animations.
    for (let i = 0; i < this.fadeOutAnimations.length; i++) {
      this.fadeOutAnimations[i].cancel();
    }
    this.fadeOutAnimations = [];

    this.clearCanvas(this.context);
    for (let i = 0; i < this.fadeOutTimeoutIds.length; i++) {
      clearTimeout(this.fadeOutTimeoutIds[i]);
    }
    this.fadeOutTimeoutIds = [];
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
