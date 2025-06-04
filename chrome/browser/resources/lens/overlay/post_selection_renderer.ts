// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert, assertInstanceof, assertNotReached} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {GLIF_HEX_COLORS} from './color_utils.js';
import {CenterRotatedBox_CoordinateType} from './geometry.mojom-webui.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import {UserAction} from './lens.mojom-webui.js';
import {INVOCATION_SOURCE} from './lens_overlay_app.js';
import {recordLensOverlayInteraction} from './metrics_utils.js';
import {getTemplate} from './post_selection_renderer.html.js';
import {ScreenshotBitmapBrowserProxyImpl} from './screenshot_bitmap_browser_proxy.js';
import {renderScreenshot} from './screenshot_utils.js';
import {focusShimmerOnRegion, ShimmerControlRequester, unfocusShimmer} from './selection_utils.js';
import type {GestureEvent} from './selection_utils.js';
import {toPercent, toPixels} from './values_converter.js';

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
  TOP_LEFT,
  TOP_RIGHT,
  BOTTOM_RIGHT,
  BOTTOM_LEFT,
}

// The amount of pixels around the edge leave as a buffer so user can't drag too
// far. Exported for testing.
export const PERIMETER_SELECTION_PADDING_PX = 4;
// The maximum length of a corner. Exported for testing.
export const MAX_CORNER_LENGTH_PX = 22;
// The maximum radius of a corner. Exported for testing.
export const MAX_CORNER_RADIUS_PX = 14;
// Cutout radius used with larger corner radii. Exported for testing.
export const CUTOUT_RADIUS_PX = 5;
// A cutout radius will only be used when the corner radius is above this
// threshold.
const CUTOUT_RADIUS_THRESHOLD_PX = 12;
// Minimum box size allowed. Exported for testing.
export const MIN_BOX_SIZE_PX = 12;
const MIN_BLUR = 8;
const MAX_BLUR = 40;

function clamp(value: number, min: number, max: number): number {
  return Math.min(Math.max(value, min), max);
}

export interface PostSelectionRendererElement {
  $: {
    backgroundImageCanvas: HTMLCanvasElement,
    postSelection: HTMLElement,
  };
}

interface CornerDimensions {
  length: number;
  radius: number;
  cutoutRadius: number;
}

const PostSelectionRendererElementBase = I18nMixin(PolymerElement);

/*
 * Renders the users visual selection after one is made. This element is also
 * responsible for allowing the user to adjust their region to issue a new
 * Lens request.
 */
export class PostSelectionRendererElement extends
    PostSelectionRendererElementBase {
  static get is() {
    return 'post-selection-renderer';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      top: {
        type: Number,
        value: 0,
      },
      left: {
        type: Number,
        value: 0,
      },
      height: {
        type: Number,
        value: 0,
      },
      width: {
        type: Number,
        value: 0,
      },
      currentDragTarget: {
        type: Number,
        value: DragTarget.NONE,
      },
      cornerIds: {
        type: Array,
        value: () => ['topLeft', 'topRight', 'bottomRight', 'bottomLeft'],
      },
      canvasHeight: Number,
      canvasWidth: Number,
      canvasPhysicalHeight: Number,
      canvasPhysicalWidth: Number,
      regionSelectedGlowEnabled: {
        type: Boolean,
        reflectToAttribute: true,
        value: () => loadTimeData.getBoolean('enableRegionSelectedGlow'),
      },
      selectionOverlayRect: Object,
      shouldDarkenScrim: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      cornerSlidersEnabled: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('cornerSlidersEnabled'),
        reflectToAttribute: true,
      },
    };
  }

  private eventTracker_: EventTracker = new EventTracker();
  // The bounds of the current selection
  declare private top: number;
  declare private left: number;
  declare private height: number;
  declare private width: number;
  // What is currently being dragged by the user.
  declare private currentDragTarget: DragTarget;
  // IDs used to generate the corner hitbox divs.
  declare private cornerIds: string[];
  declare private canvasHeight: number;
  declare private canvasWidth: number;
  declare private canvasPhysicalHeight: number;
  declare private canvasPhysicalWidth: number;
  // The bounds of the parent element. This is updated by the parent to avoid
  // this class needing to call getBoundingClientRect().
  // Whether the region selected glow is enabled via feature flag.
  declare private regionSelectedGlowEnabled: boolean;
  declare private selectionOverlayRect: DOMRect;

  private context: CanvasRenderingContext2D;
  // Listener IDs for events tracked from the browser.
  private listenerIds: number[];
  // The original bounds from the start of a drag or slider change.
  private originalBounds:
      PostSelectionBoundingBox = {left: 0, top: 0, width: 0, height: 0};
  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();
  private resizeObserver: ResizeObserver = new ResizeObserver(() => {
    this.handleResize();
  });
  private newBoxAnimation: Animation|null = null;
  private animateOnResize = false;
  // Whether to darken the post selection scrim.
  declare private shouldDarkenScrim;
  // Whether to enable corner sliders for keyboard control.
  declare private cornerSlidersEnabled: boolean;
  // Timeout for calling handleGestureEnd() after a slider change.
  private sliderChangedTimeout: number =
      loadTimeData.getValue('sliderChangedTimeout');
  // -1 if no timeout is currently running.
  private sliderChangedTimeoutID: number = -1;

  override connectedCallback() {
    super.connectedCallback();
    ScreenshotBitmapBrowserProxyImpl.getInstance().fetchScreenshot(
        (screenshot: ImageBitmap) => {
          renderScreenshot(this.$.backgroundImageCanvas, screenshot);
        });
    this.eventTracker_.add(
        document, 'render-post-selection',
        (e: CustomEvent<PostSelectionBoundingBox>) => {
          this.onRenderPostSelection(e);
        });
    this.eventTracker_.add(document, 'finished-receiving-text', () => {
      if (this.hasSelection()) {
        // Check for selectable text
        this.dispatchEvent(new CustomEvent('detect-text-in-region', {
          bubbles: true,
          composed: true,
          detail: this.getNormalizedCenterRotatedBox(),
        }));
      }
    });
    this.eventTracker_.add(document, 'text-found-in-region', () => {
      if (this.hasSelection()) {
        this.shouldDarkenScrim = true;
      }
    });
    this.resizeObserver.observe(this);
    // Set up listener to listen to events from C++.
    this.listenerIds = [
      this.browserProxy.callbackRouter.clearAllSelections.addListener(
          this.clearSelection.bind(this)),
      this.browserProxy.callbackRouter.clearRegionSelection.addListener(
          this.clearRegionSelection.bind(this)),
      this.browserProxy.callbackRouter.setPostRegionSelection.addListener(
          this.setSelection.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
    this.resizeObserver.unobserve(this);
    this.listenerIds.forEach(
        id => assert(this.browserProxy.callbackRouter.removeListener(id)));
    this.listenerIds = [];
  }

  setCanvasSizeTo(width: number, height: number) {
    // Resetting the canvas width and height also clears the canvas.
    this.canvasWidth = width;
    this.canvasHeight = height;
    this.canvasPhysicalWidth = width * window.devicePixelRatio;
    this.canvasPhysicalHeight = height * window.devicePixelRatio;
  }

  clearRegionSelection() {
    unfocusShimmer(this, ShimmerControlRequester.CURSOR);
    unfocusShimmer(this, ShimmerControlRequester.MANUAL_REGION);
    this.clearSelection();
  }

  clearSelection() {
    unfocusShimmer(this, ShimmerControlRequester.POST_SELECTION);
    this.height = 0;
    this.width = 0;
    this.shouldDarkenScrim = false;
    this.dispatchEvent(new CustomEvent(
        'hide-selected-region-context-menu', {bubbles: true, composed: true}));
    this.notifyPostSelectionUpdated();
  }

  handleGestureStart(event: GestureEvent): boolean {
    this.currentDragTarget =
        this.dragTargetFromPoint(event.startX, event.startY);

    if (this.shouldHandleGestureStart()) {
      // User is dragging the post selection (if enabled) or resizing.
      this.originalBounds = {
        left: this.left,
        top: this.top,
        width: this.width,
        height: this.height,
      };
      this.shouldDarkenScrim = false;
      return true;
    }
    return false;
  }

  handleGestureDrag(event: GestureEvent) {
    const imageBounds = this.selectionOverlayRect;
    const normalizedX = (event.clientX - imageBounds.left) / imageBounds.width;
    const normalizedY = (event.clientY - imageBounds.top) / imageBounds.height;
    const normalizedMinBoxWidth = MIN_BOX_SIZE_PX / imageBounds.width;
    const normalizedMinBoxHeight = MIN_BOX_SIZE_PX / imageBounds.height;

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
      default:
        assertNotReached();
    }
    assert(newLeft !== undefined);
    assert(newTop !== undefined);
    assert(newRight !== undefined);
    assert(newBottom !== undefined);

    // Ensure the new region is within the image bounds.
    const clampedBounds = this.getClampedBounds({
      left: newLeft,
      top: newTop,
      width: newRight - newLeft,
      height: newBottom - newTop,
    });

    // Set the new dimensions.
    this.setDimensions(
        clampedBounds.top, clampedBounds.left, clampedBounds.height,
        clampedBounds.width);

    this.rerender();
  }

  handleGestureEnd() {
    if (this.areBoundsChanging()) {
      // Issue Lens request for new bounds
      BrowserProxyImpl.getInstance().handler.issueLensRegionRequest(
          this.getNormalizedCenterRotatedBox(), /*is_click=*/ false);

      // Check for selectable text
      this.dispatchEvent(new CustomEvent('detect-text-in-region', {
        bubbles: true,
        composed: true,
        detail: this.getNormalizedCenterRotatedBox(),
      }));

      recordLensOverlayInteraction(
          INVOCATION_SOURCE, UserAction.kRegionSelectionChange);
    }

    this.originalBounds = {left: 0, top: 0, width: 0, height: 0};
    this.currentDragTarget = DragTarget.NONE;
  }

  cancelGesture() {
    this.originalBounds = {left: 0, top: 0, width: 0, height: 0};
    this.currentDragTarget = DragTarget.NONE;
  }

  handleRightClick(event: PointerEvent) {
    const boundingRect = this.$.postSelection.getBoundingClientRect();
    if (this.dragTargetFromPoint(event.clientX, event.clientY) !==
            DragTarget.NONE ||
        (event.clientX >= boundingRect.left &&
         event.clientX <= boundingRect.right &&
         event.clientY >= boundingRect.top &&
         event.clientY <= boundingRect.bottom)) {
      this.dispatchEvent(
          new CustomEvent('restore-selected-region-context-menu', {
            bubbles: true,
            composed: true,
          }));
    }
  }

  // Handle changes in the slider inputs used by keyboard users.
  handleSliderChange(event: Event) {
    if (this.sliderChangedTimeoutID <= 0) {
      // Initiating a slider change.
      this.originalBounds = {
        left: this.left,
        top: this.top,
        width: this.width,
        height: this.height,
      };
      this.shouldDarkenScrim = false;
    }

    const imageBounds = this.selectionOverlayRect;
    const normalizedMinBoxWidth = MIN_BOX_SIZE_PX / imageBounds.width;
    const normalizedMinBoxHeight = MIN_BOX_SIZE_PX / imageBounds.height;

    const currentLeft = this.left;
    const currentTop = this.top;
    const currentRight = this.left + this.width;
    const currentBottom = this.top + this.height;
    let newLeft = currentLeft;
    let newTop = currentTop;
    let newRight = currentRight;
    let newBottom = currentBottom;

    const slider = event.currentTarget;
    assertInstanceof(slider, HTMLInputElement);
    const value = Number(slider.value);
    switch (slider.dataset['cornerId']) {
      // The top left and bottom right corners' sliders control the x position
      // of their corners. Set the x position to the slider value, as long as it
      // is not below 0, above 1, or beyond the x position of the opposite side.
      case 'topLeft':
        newLeft = Math.max(
            0, Math.min(value / 100, currentRight - normalizedMinBoxWidth));
        break;
      case 'bottomRight':
        newRight = Math.min(
            1, Math.max(value / 100, currentLeft + normalizedMinBoxWidth));
        break;
      // The top right and bottom left corners' sliders control the y position
      // of their corners. Move the y position by the negative of the change in
      // the slider value, so that increasing the slider moves the cursor up
      // (i.e. in the -y direction), as long as it is not below 0, above 1, or
      // beyond the y position of the opposite side.
      case 'topRight':
        newTop = Math.max(
            0,
            Math.min(
                2 * currentTop - value / 100,
                currentBottom - normalizedMinBoxHeight));
        break;
      case 'bottomLeft':
        newBottom = Math.min(
            1,
            Math.max(
                2 * currentBottom - value / 100,
                currentTop + normalizedMinBoxHeight));
        break;
      default:
        assertNotReached();
    }
    // Ensure the new region is within the image bounds.
    const clampedBounds = this.getClampedBounds({
      left: newLeft,
      top: newTop,
      width: newRight - newLeft,
      height: newBottom - newTop,
    });

    // Set the new dimensions.
    this.setDimensions(
        clampedBounds.top, clampedBounds.left, clampedBounds.height,
        clampedBounds.width);

    this.rerender();

    // Timeout to wait for further slider changes before calling
    // handleGestureEnd().
    if (this.sliderChangedTimeoutID > 0) {
      clearTimeout(this.sliderChangedTimeoutID);
    }
    this.sliderChangedTimeoutID = setTimeout(() => {
      this.sliderChangedTimeoutID = -1;
      this.handleGestureEnd();
    }, this.sliderChangedTimeout);
  }

  private getPostSelectionStyles(): string {
    const style: string[] = [
      `--gradient-blue: ${GLIF_HEX_COLORS.blue}`,
      `--gradient-red: ${GLIF_HEX_COLORS.red}`,
      `--gradient-yellow: ${GLIF_HEX_COLORS.yellow}`,
      `--gradient-green: ${GLIF_HEX_COLORS.green}`,
    ];

    if (!this.selectionOverlayRect) {
      return style.join('; ');
    }

    const imageBounds = this.selectionOverlayRect;
    const selectionWidth = this.width * imageBounds.width;
    const selectionHeight = this.height * imageBounds.height;
    if (selectionWidth > 0 && selectionHeight > 0) {
      const minSide = Math.min(selectionWidth, selectionHeight);
      const blurAmount =
          Math.max(MIN_BLUR, Math.min(Math.round(minSide / 4), MAX_BLUR));
      style.push(`--region-selected-glow-blur-radius: ${blurAmount}px`);
    }

    return style.join('; ');
  }

  private setDimensions(
      top: number, left: number, height: number, width: number) {
    this.top = top;
    this.left = left;
    this.height = height;
    this.width = width;
    this.updateSliderValues();
  }

  // Update the attributes of the sliders to reflect the current dimensions.
  private updateSliderValues() {
    const sliders =
        this.shadowRoot!.querySelectorAll<HTMLInputElement>('input');
    for (const slider of sliders) {
      switch (slider.dataset['cornerId']) {
        case 'topLeft':
          slider.value = (this.left * 100).toString();
          slider.ariaLabel = this.i18n(
              'topLeftSliderAriaLabel', Math.round(this.left * 100),
              Math.round(this.top * 100));
          break;
        case 'topRight':
          slider.value = (this.top * 100).toString();
          slider.ariaLabel = this.i18n(
              'topRightSliderAriaLabel',
              Math.round((this.left + this.width) * 100),
              Math.round(this.top * 100));
          break;
        case 'bottomRight':
          slider.value = ((this.left + this.width) * 100).toString();
          slider.ariaLabel = this.i18n(
              'bottomRightSliderAriaLabel',
              Math.round((this.left + this.width) * 100),
              Math.round((this.top + this.height) * 100));
          break;
        case 'bottomLeft':
          slider.value = ((this.top + this.height) * 100).toString();
          slider.ariaLabel = this.i18n(
              'bottomLeftSliderAriaLabel', Math.round(this.left * 100),
              Math.round((this.top + this.height) * 100));
          break;
        default:
          assertNotReached();
      }
    }
  }

  private setSelection(region: CenterRotatedBox) {
    const normalizedTop = region.box.y - (region.box.height / 2);
    const normalizedLeft = region.box.x - (region.box.width / 2);

    this.setDimensions(
        normalizedTop, normalizedLeft, region.box.height, region.box.width);
    this.originalBounds = {left: 0, top: 0, width: 0, height: 0};

    this.rerender();
    this.triggerNewBoxAnimation();
  }

  private onRenderPostSelection(e: CustomEvent<PostSelectionBoundingBox>) {
    this.setDimensions(
        e.detail.top, e.detail.left, e.detail.height, e.detail.width);
    this.rerender();
    this.triggerNewBoxAnimation();
  }

  // Returns the bounds of the post selection clamped to the edges of the image,
  // including the post selection corners. If no bounds are given, uses those
  // currently being rendered.
  private getClampedBounds(bounds?: PostSelectionBoundingBox):
      PostSelectionBoundingBox {
    const imageBounds = this.selectionOverlayRect;
    const left = bounds ? bounds.left : this.left;
    const top = bounds ? bounds.top : this.top;
    const right = bounds ? bounds.left + bounds.width : this.left + this.width;
    const bottom = bounds ? bounds.top + bounds.height : this.top + this.height;

    // Helper values to clamp to within the bounds.
    const normalizedMinBoxWidth = MIN_BOX_SIZE_PX / imageBounds.width;
    const normalizedMinBoxHeight = MIN_BOX_SIZE_PX / imageBounds.height;
    const normalizedPerimeterPaddingWidth =
        PERIMETER_SELECTION_PADDING_PX / imageBounds.width;
    const normalizedPerimeterPaddingHeight =
        PERIMETER_SELECTION_PADDING_PX / imageBounds.height;
    const minXValue = normalizedPerimeterPaddingWidth;
    const minYValue = normalizedPerimeterPaddingHeight;
    const maxXValue = 1 - normalizedPerimeterPaddingWidth;
    const maxYValue = 1 - normalizedPerimeterPaddingHeight;

    // Clamp the values to within the selection overlay bounds.
    const clampedLeft =
        clamp(left, minXValue, maxXValue - normalizedMinBoxWidth);
    const clampedTop =
        clamp(top, minYValue, maxYValue - normalizedMinBoxHeight);
    const clampedRight =
        clamp(right, minXValue + normalizedMinBoxWidth, maxXValue);
    const clampedBottom =
        clamp(bottom, minYValue + normalizedMinBoxHeight, maxYValue);

    return {
      left: clampedLeft,
      top: clampedTop,
      width: clampedRight - clampedLeft,
      height: clampedBottom - clampedTop,
    };
  }

  private handleResize() {
    // Only update properties defined absolutely, i.e. corner dimensions.
    // Properties that are defined relatively do not need to be updated.
    this.updateCornerDimensions();
    if (this.newBoxAnimation) {
      (this.newBoxAnimation.effect as KeyframeEffect)
          .setKeyframes(this.getNewBoxAnimationKeyframes());
    } else if (this.animateOnResize) {
      this.animateOnResize = false;
      this.triggerNewBoxAnimation();
    }
    this.rerender();
  }

  private rerender() {
    // rerender() can be called when there is not a selection present. This
    // should be a no-op otherwise the shimmer will be set to focus on the post
    // selection region without a selection.
    if (!this.hasSelection()) {
      return;
    }

    const clampedBounds = this.getClampedBounds();
    // Set the CSS properties to reflect current bounds and force rerender.
    this.style.setProperty('--selection-width', toPercent(clampedBounds.width));
    this.style.setProperty(
        '--selection-height', toPercent(clampedBounds.height));
    this.style.setProperty('--selection-top', toPercent(clampedBounds.top));
    this.style.setProperty('--selection-left', toPercent(clampedBounds.left));

    this.updateCornerDimensions();

    // Focus the shimmer on the new post selection region.
    focusShimmerOnRegion(
        this, clampedBounds.top, clampedBounds.left, clampedBounds.width,
        clampedBounds.height, ShimmerControlRequester.POST_SELECTION);

    this.notifyPostSelectionUpdated();
  }

  private updateCornerDimensions() {
    const cornerDimensions = this.getCornerDimensions();
    this.style.setProperty(
        '--post-selection-corner-horizontal-length',
        toPixels(cornerDimensions.length));
    this.style.setProperty(
        '--post-selection-corner-vertical-length',
        toPixels(cornerDimensions.length));
    this.style.setProperty(
        '--post-selection-corner-radius', toPixels(cornerDimensions.radius));
    this.style.setProperty(
        '--post-selection-cutout-corner-radius',
        toPixels(cornerDimensions.cutoutRadius));
  }

  private triggerNewBoxAnimation() {
    const parentBoundingRect = this.selectionOverlayRect;
    if (parentBoundingRect.width === 0 || parentBoundingRect.height === 0) {
      // Renderer has probably not been sized yet. Defer until resize.
      this.animateOnResize = true;
      return;
    }

    this.newBoxAnimation = this.animate(this.getNewBoxAnimationKeyframes(), {
      duration: 450,
      easing: 'cubic-bezier(0.2, 0.0, 0, 1.0)',
    });
    this.newBoxAnimation.onfinish = () => {
      this.newBoxAnimation = null;
    };
  }

  private getNewBoxAnimationKeyframes() {
    const parentBoundingRect = this.selectionOverlayRect;
    const cornerDimensions = this.getCornerDimensions();
    return [
      {
        [`--post-selection-corner-horizontal-length`]:
            toPixels(parentBoundingRect.width * this.width / 2),
        [`--post-selection-corner-vertical-length`]:
            toPixels(parentBoundingRect.height * this.height / 2),
      },
      {
        [`--post-selection-corner-horizontal-length`]:
            toPixels(cornerDimensions.length),
        [`--post-selection-corner-vertical-length`]:
            toPixels(cornerDimensions.length),
      },
    ];
  }

  private getCornerDimensions(): CornerDimensions {
    const imageBounds = this.selectionOverlayRect;
    if (imageBounds.width === 0 || imageBounds.height === 0) {
      // Renderer has probably not been sized yet. Return default values.
      return {
        length: MAX_CORNER_LENGTH_PX,
        radius: MAX_CORNER_RADIUS_PX,
        cutoutRadius: CUTOUT_RADIUS_PX,
      };
    }

    const shortestSide = Math.min(
        this.width * imageBounds.width, this.height * imageBounds.height);
    const length = Math.min(shortestSide / 2, MAX_CORNER_LENGTH_PX);
    const radius = Math.min(shortestSide / 3, MAX_CORNER_RADIUS_PX);
    // Do not use a cutout radius at small radii to prevent gaps.
    const cutoutRadius =
        radius > CUTOUT_RADIUS_THRESHOLD_PX ? CUTOUT_RADIUS_PX : 0;

    return {length, radius, cutoutRadius};
  }

  private notifyPostSelectionUpdated() {
    this.dispatchEvent(new CustomEvent('post-selection-updated', {
      bubbles: true,
      composed: true,
      detail: {
        top: this.top,
        left: this.left,
        width: this.width,
        height: this.height,
        centerRotatedBox: this.getNormalizedCenterRotatedBox(),
      },
    }));
  }

  // Returns if the current bounds are being updated.
  private areBoundsChanging() {
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
      return (el instanceof HTMLElement) &&
          el.classList.contains('corner-hit-box');
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
      default:
        // Did not click on a target we care about.
        break;
    }
    return DragTarget.NONE;
  }

  private shouldHandleGestureStart(): boolean {
    return this.currentDragTarget !== DragTarget.NONE;
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
  hasSelection(): boolean {
    return this.width > 0 && this.height > 0;
  }

  setSelectionOverlayRectForTesting(rect: DOMRect) {
    this.selectionOverlayRect = rect;
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
  initialValue: '14px',
});
