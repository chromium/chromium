// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getFallbackTheme, getShaderLayerColorHexes, GLIF_HEX_COLORS} from './color_utils.js';
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

function fullscreenNormalizedCenterRotatedBox(): CenterRotatedBox {
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

function fullscreenPostSelectionRegion(): PostSelectionBoundingBox {
  return {
    top: 0,
    left: 0,
    width: 1,
    height: 1,
  };
}

export interface RegionSelectionElement {
  $: {
    highlightImgCanvas: HTMLCanvasElement,
    keyboardSelection: HTMLDivElement,
    regionSelectionCanvas: HTMLCanvasElement,
  };
}

const RegionSelectionElementBase = I18nMixin(PolymerElement);

/*
 * Element responsible for rendering the region being selected by the user. It
 * does not render any post-selection state.
 */
export class RegionSelectionElement extends RegionSelectionElementBase {
  static get is() {
    return 'region-selection';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      borderGlowEnabled: {
        reflectToAttribute: true,
        type: Boolean,
        value: false,
      },
      canvasHeight: Number,
      canvasWidth: Number,
      canvasPhysicalHeight: Number,
      canvasPhysicalWidth: Number,
      displayKeyboardSelection: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableKeyboardSelection'),
        reflectToAttribute: true,
      },
      hasSelected: {
        reflectToAttribute: true,
        type: Boolean,
        value: false,
      },
      isSelecting: {
        reflectToAttribute: true,
        type: Boolean,
        value: false,
      },
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

  private eventTracker_: EventTracker = new EventTracker();
  // Whether the border glow is enabled. This is a replacement for the shimmer.
  declare private borderGlowEnabled: boolean;
  declare private canvasHeight: number;
  declare private canvasWidth: number;
  declare private canvasPhysicalHeight: number;
  declare private canvasPhysicalWidth: number;
  // Whether the user has selected a region.
  declare private hasSelected: boolean;
  // Whether the user is currently selecting a region.
  declare private isSelecting: boolean;
  private context: CanvasRenderingContext2D;
  // The data URI of the current overlay screenshot.
  declare private screenshotDataUri: string;
  // Shader hex colors.
  declare private shaderLayerColorHexes: string[];
  // The overlay theme.
  declare private theme: OverlayTheme;
  // The bounds of the parent element. This is updated by the parent to avoid
  // this class needing to call getBoundingClientRect()
  declare private selectionOverlayRect: DOMRect;
  // Whether keyboard selection should be displayed.
  declare private displayKeyboardSelection: boolean;

  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();

  private readonly gradientRegionStrokeEnabled: boolean =
      loadTimeData.getBoolean('enableGradientRegionStroke');
  private readonly whiteRegionStrokeEnabled: boolean =
      loadTimeData.getBoolean('enableWhiteRegionStroke');
  // The tap region dimensions are the height and width that the region should
  // have when the user taps instead of drag.
  private readonly tapRegionHeight: number =
      loadTimeData.getInteger('tapRegionHeight');
  private readonly tapRegionWidth: number =
      loadTimeData.getInteger('tapRegionWidth');
  private readonly enableKeyboardSelection: boolean =
      loadTimeData.getBoolean('enableKeyboardSelection');

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
    ScreenshotBitmapBrowserProxyImpl.getInstance().addOnOverlayReshownListener(
        (screenshot: ImageBitmap) => {
          renderScreenshot(this.$.highlightImgCanvas, screenshot);
        });
    if (this.enableKeyboardSelection) {
      this.eventTracker_.add(
          document, 'post-selection-updated', (e: CustomEvent) => {
            this.displayKeyboardSelection =
                e.detail.height === 0 && e.detail.width === 0;
          });
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  private computeShaderLayerColorHexes_() {
    return getShaderLayerColorHexes(this.theme);
  }

  handleGestureStart() {
    this.isSelecting = true;
    this.hasSelected = false;
  }

  // Handles a drag gesture by drawing a bounded box on the canvas.
  handleGestureDrag(event: GestureEvent) {
    this.clearCanvas();
    this.renderBoundingBox(event);
  }

  handleGestureEnd(event: GestureEvent): boolean {
    const isClick = event.state === GestureState.STARTING;
    const box = this.getNormalizedCenterRotatedBoxFromGesture(event);
    const region = this.getPostSelectionRegion(event);
    const interaction =
        isClick ? UserAction.kTapRegionSelection : UserAction.kRegionSelection;
    this.issueRequest(isClick, box, region, interaction);
    return true;
  }

  private onKeyboardSelection(event: Event): boolean {
    if (event instanceof KeyboardEvent &&
        !(event.key === 'Enter' || event.key === ' ')) {
      return false;
    }

    this.issueRequest(/*isClick=*/ false,
                      fullscreenNormalizedCenterRotatedBox(),
                      fullscreenPostSelectionRegion(),
                      UserAction.kFullScreenshotRegionSelection);
    return true;
  }

  private issueRequest(
      isClick: boolean, box: CenterRotatedBox, region: PostSelectionBoundingBox,
      interaction: UserAction) {
    recordLensOverlayInteraction(INVOCATION_SOURCE, interaction);
    // Issue the Lens request.
    this.browserProxy.handler.issueLensRegionRequest(box, isClick);

    // Relinquish control from the shimmer.
    unfocusShimmer(this, ShimmerControlRequester.MANUAL_REGION);

    // Keep the region rendered on the page
    this.dispatchEvent(new CustomEvent('render-post-selection', {
      bubbles: true,
      composed: true,
      detail: region,
    }));

    // Check for selectable text
    this.dispatchEvent(new CustomEvent('detect-text-in-region', {
      bubbles: true,
      composed: true,
      detail: box,
    }));

    this.clearCanvas();

    this.hasSelected = true;
    this.isSelecting = false;
  }

  // Fade out scrim after drag to resize selection in post selection renderer
  // TODO(crbug.com/420998632): Move scrim out to a central component so that
  // post selection drag handling is not dependent on the region selection scrim
  handlePostSelectionDragGestureEnd(): void {
    this.hasSelected = true;
    this.isSelecting = false;
  }

  handlePostSelectionCleared(): void {
    this.hasSelected = false;
    this.isSelecting = false;
  }

  cancelGesture() {
    this.clearCanvas();

    this.isSelecting = false;
    this.hasSelected = false;
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
    const centerX = (left + right) / 2;
    const centerY = (top + bottom) / 2;

    // Get the vertical and horizontal directions of the drag.
    const isDraggingDown = relativeDragEnd.y > relativeDragStart.y;
    const isDraggingRight = relativeDragEnd.x > relativeDragStart.x;

    let gradient;
    if (this.gradientRegionStrokeEnabled) {
      // Use AIM style GLIF color gradient.
      gradient = this.context.createConicGradient(0, centerX, centerY);
      gradient.addColorStop(0, GLIF_HEX_COLORS.blue);
      gradient.addColorStop(0.45, GLIF_HEX_COLORS.blue);
      gradient.addColorStop(0.6, GLIF_HEX_COLORS.red);
      gradient.addColorStop(0.76, GLIF_HEX_COLORS.yellow);
      gradient.addColorStop(0.92, GLIF_HEX_COLORS.green);
    } else if (this.whiteRegionStrokeEnabled) {
      // Use white gradient.
      gradient = this.context.createLinearGradient(
          left,
          bottom,
          right,
          top,
      );
      gradient.addColorStop(0, 'rgba(255, 255, 255, 1)');
      gradient.addColorStop(0.92, 'rgba(255, 255, 255, 0.5)');
    } else {
      // Use dynamic theme color gradient.
      gradient = this.context.createLinearGradient(
          left,
          bottom,
          right,
          top,
      );
      gradient.addColorStop(0, this.shaderLayerColorHexes[0]);
      gradient.addColorStop(0.5, this.shaderLayerColorHexes[1]);
      gradient.addColorStop(1, this.shaderLayerColorHexes[2]);
    }

    const strokeWidth = 3;
    this.context.lineWidth = strokeWidth;
    this.context.strokeStyle = gradient;

    // Step 1: Define the path for the main clipping region (the 'hole' in the
    // scrim)
    this.context.beginPath();
    // The corner corresponding to the user's cursor should have 0 radius.
    const radii = [
      isDraggingDown || isDraggingRight ? idealCornerRadius : 0,
      isDraggingDown || !isDraggingRight ? idealCornerRadius : 0,
      !isDraggingDown || !isDraggingRight ? idealCornerRadius : 0,
      !isDraggingDown || isDraggingRight ? idealCornerRadius : 0,
    ];
    this.context.roundRect(left, top, width, height, radii);

    // Step 2: Save the context and apply the clip. This clip ensures
    // the image and stroke are only drawn within this bounded region.
    this.context.save();
    this.context.clip();

    // Step 3: Draw the highlight image. It will be clipped to the path.
    this.context.drawImage(
        this.$.highlightImgCanvas, 0, 0, this.canvasWidth, this.canvasHeight);

    // Step 4: Draw the stroke
    if (this.gradientRegionStrokeEnabled) {
      // Draw the stroke *inside* the clipped region.
      // To achieve this, we create a new path slightly inset from the clip
      // boundary.
      this.context.beginPath();
      const inset = strokeWidth / 2;

      const strokeRectLeft = left + inset;
      const strokeRectTop = top + inset;
      const strokeRectWidth = width - (inset * 2);
      const strokeRectHeight = height - (inset * 2);

      // Adjust radii for the stroke path to keep it visually consistent
      const strokeRadii = radii.map(r => Math.max(0, r - inset));

      this.context.roundRect(
          strokeRectLeft, strokeRectTop, strokeRectWidth, strokeRectHeight,
          strokeRadii);
      // Stroke this new path. Since the clip is active,
      // this stroke will be confined to the original clip region.
      this.context.stroke();
      this.context.restore();
    } else {
      this.context.restore();
      // Stroke the path on top of the image.
      this.context.stroke();
    }

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
   * normalized to the selection overlay dimensions. The gesture is
   * expected to be a drag.
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
    return gesture.state === GestureState.STARTING ?
        this.getPostSelectionRegionFromTap(gesture) :
        this.getPostSelectionRegionFromDrag(gesture);
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
