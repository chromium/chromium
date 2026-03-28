// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {skColorToHexColor} from '//resources/js/color_utils.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getFallbackTheme} from './color_utils.js';
import type {OverlayTheme} from './lens.mojom-webui.js';
import type {OverlayBorderGlowElement} from './overlay_border_glow.js';
import type {OverlayShimmerCanvasElement} from './overlay_shimmer_canvas.js';
import type {PostSelectionBoundingBox, PostSelectionRendererElement} from './post_selection_renderer.js';
import type {RegionSelectionElement} from './region_selection.js';
import {ScreenshotBitmapBrowserProxyImpl} from './screenshot_bitmap_browser_proxy.js';
import {renderScreenshot} from './screenshot_utils.js';
import type {SelectedRegion} from './selection_overlay_base_handler.js';
import {SelectionOverlayBaseHandler} from './selection_overlay_base_handler.js';
import {CursorType, DRAG_THRESHOLD, DragFeature, emptyGestureEvent, focusShimmerOnRegion, GestureState, ShimmerControlRequester} from './selection_utils.js';
import type {GestureEvent, OverlayShimmerFocusedRegion} from './selection_utils.js';

// The amountf of margins in pixels to add to the screenshot when the window is
// resized.
const SCREENSHOT_FULLSIZE_MARGIN_PIXEL = 24;

// The number of pixels the screenshot can differ from the viewport before
// adding margins.
const SCREENSHOT_RESIZE_TOLERANCE_PIXELS = 2;

// The size of our custom cursor.
export const CURSOR_SIZE_PIXEL = 32;

// The cursor image url css variable name.
export const CURSOR_IMG_URL = '--cursor-img-url';

export interface CursorData {
  cursor: CursorType;
}

const SelectionOverlayElementBase = I18nMixinLit(CrLitElement);

/*
 * Element responsible for coordinating selections between the various selection
 * features. This includes:
 *   - Storing state needed to coordinate selections across features
 *   - Listening to mouse/tap events and delegating them to the correct features
 *   - Coordinating animations between the different features
 */
export abstract class SelectionOverlayBaseLitElement extends
    SelectionOverlayElementBase {
  static override get properties() {
    return {
      isScreenshotRendered: {
        type: Boolean,
        reflect: true,
      },
      isResized: {
        type: Boolean,
        reflect: true,
      },
      isInitialSize: {
        type: Boolean,
        reflect: true,
      },
      canvasHeight: {type: Number},
      canvasWidth: {type: Number},
      isPointerInside: {type: Boolean},
      currentGesture: {type: Object},
      disableShimmer: {
        type: Boolean,
      },
      enableBorderGlow: {
        type: Boolean,
      },
      isClosing: {
        type: Boolean,
        reflect: true,
      },
      shimmerOnSegmentation: {
        type: Boolean,
        reflect: true,
      },
      shimmerFadeOutComplete: {
        type: Boolean,
        reflect: true,
      },
      darkenExtraScrim: {
        type: Boolean,
        reflect: true,
      },
      theme: {type: Object},
      selectionOverlayRect: {type: Object},
      hideBackgroundImageCanvas: {
        type: Boolean,
        reflect: true,
      },
      sidePanelOpened: {
        type: Boolean,
        reflect: true,
      },
      activeRegionId: {
        type: String,
      },
    };
  }

  // Whether the screenshot has finished loading in.
  accessor isScreenshotRendered: boolean = false;
  // Whether the selection overlay is its initial size, or has changed size.
  accessor isResized: boolean = false;
  accessor isInitialSize: boolean = true;
  // Width and height values for rendering the background image canvas as the
  // proper dimensions.
  protected accessor canvasHeight: number = 0;
  protected accessor canvasWidth: number = 0;
  // The current content rectangle of the selection elements DIV. This is the
  // bounds of the screenshot and the part the user interacts with. This should
  // be used instead of call getBoundingClientRect().
  protected accessor selectionOverlayRect: DOMRect = new DOMRect();

  // The current gesture event. The coordinate values are only accurate if a
  // gesture has started.
  protected accessor currentGesture: GestureEvent = emptyGestureEvent();
  protected accessor disableShimmer: boolean =
      !loadTimeData.getBoolean('enableShimmer');
  protected accessor enableBorderGlow: boolean =
      loadTimeData.getBoolean('enableBorderGlow');
  // Whether the overlay is being shut down.
  protected accessor isClosing: boolean = false;
  // Whether the default background scrim is currently being darkened.
  protected accessor darkenExtraScrim: boolean = false;
  // Whether the shimmer is currently focused on a segmentation mask.
  protected accessor shimmerOnSegmentation: boolean = false;
  protected accessor shimmerFadeOutComplete: boolean = true;
  // Whether the side panel is currently opened.
  protected accessor sidePanelOpened: boolean = false;
  // Whether the background image canvas should currently be shown.
  protected accessor hideBackgroundImageCanvas: boolean = false;

  protected accessor isPointerInside: boolean = false;

  protected accessor theme: OverlayTheme = getFallbackTheme();
  accessor activeRegionId: string = '';

  protected eventTracker_: EventTracker = new EventTracker();
  // Listener ids for events from the browser side.
  protected listenerIds: number[] = [];
  private onOverlayReshownListenerId?: number;
  // The feature currently being dragged. Once a feature responds to a drag
  // event, no other feature will receive gesture events.
  protected draggingRespondent = DragFeature.NONE;
  private resizeObserver: ResizeObserver =
      new ResizeObserver(this.handleResize.bind(this));
  // Used to listen for changes in the window.devicePixelRatio. Stored as a
  // variable so we can easily add and remove the listener.
  private matchMedia?: MediaQueryList;
  protected cursorOffsetX: number = 3;
  protected cursorOffsetY: number = 6;
  private hasInitialFlashAnimationEnded = false;
  protected baseHandler: SelectionOverlayBaseHandler =
      SelectionOverlayBaseHandler.getInstance();
  private regions: SelectedRegion[] = [];
  private creationOrder: string[] = [];

  // The ID returned by requestAnimationFrame for the updateCursorPosition,
  // onPointerMove, and handleResize functions.
  private updateCursorPositionRequestId?: number;
  private onPointerMoveRequestId?: number;
  private handleResizeRequestId?: number;

  getSelectionElementColor(): string {
    const theme = this.theme;
    if (!theme?.selectionElement) {
      return '';
    }
    return skColorToHexColor(theme.selectionElement);
  }

  getPrimaryColor(): string {
    const theme = this.theme;
    if (!theme?.primary) {
      return '';
    }
    return skColorToHexColor(theme.primary);
  }

  abstract get selectionElements(): {
    backgroundImageCanvas: HTMLCanvasElement,
    cursor: HTMLElement,
    initialFlashScrim: HTMLElement,
    overlayShimmerCanvas: OverlayShimmerCanvasElement,
    postSelectionRenderer: PostSelectionRendererElement,
    regionSelectionLayer: RegionSelectionElement,
    selectionOverlay: HTMLElement,
  };

  override connectedCallback() {
    super.connectedCallback();
    this.resizeObserver.observe(this);
    this.listenerIds = [
      this.baseHandler.addNotifyOverlayClosingListener(() => {
        this.isClosing = true;
        this.removeDragListeners();
      }),
      this.baseHandler.addMultiRegionSelectionListener((regions) => {
        // Identify if a new region has been added by comparing against the
        // previous set of IDs.
        const currentIds = new Set(regions.map(r => r.id));
        const oldIds = new Set(this.regions.map(r => r.id));
        const newRegion = regions.find(r => !oldIds.has(r.id));
        this.regions = regions;

        // Maintain the order in which regions were created to provide a
        // predictable sequential fallback when regions are deleted.
        if (newRegion) {
          this.creationOrder.push(newRegion.id);
        }
        this.creationOrder =
            this.creationOrder.filter(id => currentIds.has(id));

        if (regions.length === 0) {
          // Reset state if all regions are cleared.
          this.activeRegionId = '';
          this.creationOrder = [];
        } else if (newRegion) {
          // Automatically focus any newly created region.
          this.activeRegionId = newRegion.id;
        } else if (!currentIds.has(this.activeRegionId)) {
          // If the currently active region was deleted, fall back to the most
          // recently created remaining region.
          this.activeRegionId =
              this.creationOrder[this.creationOrder.length - 1] ||
              regions[0].id;
        }
      }),
    ];
    ScreenshotBitmapBrowserProxyImpl.getInstance().fetchScreenshot(
        this.screenshotDataReceived.bind(this));
    this.onOverlayReshownListenerId =
        ScreenshotBitmapBrowserProxyImpl.getInstance()
            .addOnOverlayReshownListener(this.onOverlayReshown.bind(this));
    this.eventTracker_.add(
        document, 'shimmer-fade-out-complete', (e: CustomEvent<boolean>) => {
          this.shimmerFadeOutComplete = e.detail;
        });
    this.eventTracker_.add(
        document, 'set-cursor', (e: CustomEvent<CursorData>) => {
          switch (e.detail.cursor) {
            case CursorType.POINTER:
              this.setCursorToPointer();
              break;
            case CursorType.CROSSHAIR:
              this.setCursorToCrosshair();
              break;
            case CursorType.TEXT:
              this.setCursorToText();
              break;
            default:
              this.resetCursor();
              break;
          }
        });
    this.eventTracker_.add(document, 'darken-extra-scrim-opacity', () => {
      this.darkenExtraScrim = true;
    });
    this.eventTracker_.add(document, 'lighten-extra-scrim-opacity', () => {
      this.darkenExtraScrim = false;
    });

    this.eventTracker_.add(
        document, 'focus-region',
        (e: CustomEvent<OverlayShimmerFocusedRegion>) => {
          if (e.detail.requester === ShimmerControlRequester.SEGMENTATION) {
            this.shimmerOnSegmentation = true;
          }
        });
    this.eventTracker_.add(document, 'unfocus-region', () => {
      this.shimmerOnSegmentation = false;
    });
    if (this.enableBorderGlow) {
      this.eventTracker_.add(
          document, 'post-selection-updated',
          (e: CustomEvent<PostSelectionBoundingBox>) => {
            this.handlePostSelectionUpdated(e.detail.height, e.detail.width);
          });
    }

    this.updateSelectionOverlayRect();
    this.updateDevicePixelRatioListener();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.resizeObserver.unobserve(this);
    this.eventTracker_.removeAll();
    this.listenerIds.forEach(id => assert(this.baseHandler.removeListener(id)));
    this.listenerIds = [];

    if (this.onOverlayReshownListenerId !== undefined) {
      ScreenshotBitmapBrowserProxyImpl.getInstance()
          .removeOnOverlayReshownListener(this.onOverlayReshownListenerId);
      this.onOverlayReshownListenerId = undefined;
    }

    if (this.matchMedia) {
      this.matchMedia = undefined;
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('isResized')) {
      this.onIsResizedChanged(this.isResized);
    }

    if (changedProperties.has('activeRegionId')) {
      this.baseHandler.activeRegionId = this.activeRegionId;
    }
  }

  override firstUpdated() {
    this.addEventListener('pointerdown', this.onPointerDown.bind(this));
    this.addEventListener('pointermove', this.updateCursorPosition.bind(this));
    this.eventTracker_.add(
        this.selectionElements.initialFlashScrim, 'animationend',
        (event: AnimationEvent) => {
          // The flash animation is the longest animation.
          if (event.animationName !== 'initial-inset-animation') {
            return;
          }
          this.onInitialFlashAnimationEnd();
        });
  }

  protected onActivateRegion(event: CustomEvent<{id: string}>) {
    this.activeRegionId = event.detail.id;
    event.stopPropagation();
  }

  private addDragListeners() {
    this.eventTracker_.add(document, 'pointerup', this.onPointerUp);
    this.eventTracker_.add(document, 'pointermove', this.onPointerMove);
    this.eventTracker_.add(document, 'pointercancel', this.onPointerCancel);
  }

  private removeDragListeners() {
    this.eventTracker_.remove(document, 'pointerup');
    this.eventTracker_.remove(document, 'pointermove');
    this.eventTracker_.remove(document, 'pointercancel');
  }

  private updateDevicePixelRatioListener() {
    // Remove the previous listener since we are now listening for a different
    // pixel ratio change.
    if (this.matchMedia) {
      this.eventTracker_.remove(this.matchMedia, 'change');
    }

    // Listen to changes to the current device pixel ratio.
    const queryString = `(resolution: ${window.devicePixelRatio}dppx)`;
    this.matchMedia = matchMedia(queryString);
    this.eventTracker_.add(
        this.matchMedia, 'change', this.onDevicePixelRatioChanged.bind(this));
  }

  private onDevicePixelRatioChanged() {
    // Update the listener to the new pixel ratio.
    this.updateDevicePixelRatioListener();
    // Resize the canvases to take the new pixel ratio change.\
    this.resizeSelectionCanvases(
        this.selectionOverlayRect.width, this.selectionOverlayRect.height);
  }

  private updateCursorPosition(event: PointerEvent) {
    // Cancel a pending event to prevent multiple updates per frame.
    if (this.updateCursorPositionRequestId) {
      cancelAnimationFrame(this.updateCursorPositionRequestId);
    }

    // Use requestAnimationFrame to only update the cursor once a frame instead
    // of multiple times per frame. This helps ensure the cursor is being
    // updated to the latest received pointer event.
    this.updateCursorPositionRequestId = requestAnimationFrame(() => {
      const mouseX = event.clientX;
      const mouseY = event.clientY;

      const cursorOffsetX = mouseX + this.cursorOffsetX;
      const cursorOffsetY = mouseY + this.cursorOffsetY;

      if (!this.disableShimmer && this.currentGesture &&
          (this.isPointerInside ||
           this.currentGesture.state === GestureState.DRAGGING)) {
        this.updateShimmerForCursor(cursorOffsetX, cursorOffsetY);
      }

      this.selectionElements.cursor.style.transform =
          `translate3d(${cursorOffsetX}px, ${cursorOffsetY}px, 0)`;
      this.updateCursorPositionRequestId = undefined;
    });
  }

  private updateShimmerForCursor(cursorLeft: number, cursorTop: number) {
    if (!this.selectionOverlayRect) {
      return;
    }
    const relativeXPercent =
        Math.max(
            0,
            Math.min(cursorLeft, this.selectionOverlayRect.right) -
                this.selectionOverlayRect.left) /
        this.selectionOverlayRect.width;
    const relativeYPercent =
        Math.max(
            0,
            Math.min(cursorTop, this.selectionOverlayRect.bottom) -
                this.selectionOverlayRect.top) /
        this.selectionOverlayRect.height;

    focusShimmerOnRegion(
        this, relativeYPercent, relativeXPercent,
        CURSOR_SIZE_PIXEL / this.selectionOverlayRect.width,
        CURSOR_SIZE_PIXEL / this.selectionOverlayRect.height,
        ShimmerControlRequester.CURSOR);
  }

  protected onIsResizedChanged(newValue: boolean): void {
    this.baseHandler.setLiveBlur(newValue);
  }

  protected getHiddenCursorClass(
      isPointerInside: boolean, state?: GestureState): string {
    // Always show when dragging, even if outside the selection overlay.
    if (!isPointerInside && state !== GestureState.DRAGGING) {
      return 'hidden';
    } else {
      return '';
    }
  }

  // LINT.IfChange(CursorOffsetValues)
  // Called on text hover and drag.
  protected setCursorToText() {
    // Set body cursor style to handle dragging.
    document.body.style.cursor = 'text';
    this.cursorOffsetX = 3;
    this.cursorOffsetY = 8;
    this.style.setProperty(CURSOR_IMG_URL, 'url("text.svg")');
  }

  // Called on region selection drag.
  protected setCursorToCrosshair() {
    // Set body cursor style to handle dragging.
    document.body.style.cursor = 'crosshair';
    this.cursorOffsetX = 3;
    this.cursorOffsetY = 6;
    this.style.setProperty(CURSOR_IMG_URL, this.defaultCursorIconUrl);
  }

  // Called on object hover.
  protected setCursorToPointer() {
    // No dragging for objects, so no need to set body cursor style.
    this.cursorOffsetX = 11;
    this.cursorOffsetY = 17;
    this.style.setProperty(CURSOR_IMG_URL, this.defaultCursorIconUrl);
  }

  protected resetCursor() {
    document.body.style.cursor = 'unset';
    this.cursorOffsetX = 3;
    this.cursorOffsetY = 6;
    this.style.setProperty(CURSOR_IMG_URL, this.defaultCursorIconUrl);
  }

  protected get defaultCursorIconUrl() {
    return 'url("lens.svg")';
  }
  // LINT.ThenChange(//chrome/browser/resources/lens/overlay/cursor_tooltip.ts:CursorOffsetValues)


  private onImageRendered() {
    // Let the parent know it is safe to blur the background.
    this.fire('screenshot-rendered', {isSidePanelOpen: this.sidePanelOpened});
    this.baseHandler.notifyOverlayInitialized();
  }

  protected onPointerenter() {
    this.isPointerInside = true;
  }

  protected onPointerleave() {
    this.isPointerInside = false;
  }

  protected onPointerDown(event: PointerEvent) {
    if (this.shouldIgnoreEvent(event)) {
      return;
    }

    if (event.button === 2 /* right button */) {
      this.handleRightClick(event);
      return;
    }

    this.addDragListeners();

    this.currentGesture = {
      state: GestureState.NOT_STARTED,
      startX: event.clientX,
      startY: event.clientY,
      clientX: event.clientX,
      clientY: event.clientY,
    };

    this.pointerDownHandled();
  }

  protected pointerDownHandled() {}

  protected handleRightClick(event: PointerEvent) {
    this.selectionElements.postSelectionRenderer.handleRightClick(event);
  }

  protected onPointerUp = (event: PointerEvent) => {
    if (!this.currentGesture) {
      return;
    }
    this.updateGestureCoordinates(event);

    if (this.currentGesture.state === GestureState.NOT_STARTED) {
      this.handleGestureStart();
    }

    if (this.currentGesture.state === GestureState.DRAGGING) {
      // Cancel the animation frame and handle the drag event immediately so
      // handleGestureEnd is not in an unexpected state.
      this.cancelPendingDragAnimationFrame();
      this.handleGestureDrag(event);
    }

    // Allow the clients to respond to the gesture, IFF a gesture has started.
    if (this.currentGesture.state !== GestureState.NOT_STARTED) {
      this.handleGestureEnd();
    }

    // After features have responded to the event, reset the current drag state.
    this.currentGesture = emptyGestureEvent();
    this.draggingRespondent = DragFeature.NONE;
    this.removeDragListeners();
  };

  protected onPointerMove = (event: PointerEvent) => {
    if (!this.currentGesture) {
      return;
    }
    this.updateGestureCoordinates(event);

    // Ignore the event if the user isn't explicitly dragging yet.
    if (!this.isDragging()) {
      return;
    }

    if (this.currentGesture.state === GestureState.NOT_STARTED) {
      // If a gesture hasn't started, start the gesture now that the user is
      // dragging.
      this.handleGestureStart();
    }

    if (this.currentGesture.state === GestureState.STARTING) {
      // If the gesture just started, move into the dragging state.
      this.currentGesture = {
        ...this.currentGesture,
        state: GestureState.DRAGGING,
      };
    }

    // If we haven't exited early, we must be in the dragging state.
    assert(this.currentGesture.state === GestureState.DRAGGING);

    // Handle the drag.
    this.cancelPendingDragAnimationFrame();
    this.onPointerMoveRequestId = requestAnimationFrame(() => {
      this.handleGestureDrag(event);
      this.onPointerMoveRequestId = undefined;
    });
  };

  private cancelPendingDragAnimationFrame() {
    if (this.onPointerMoveRequestId) {
      cancelAnimationFrame(this.onPointerMoveRequestId);
    }
  }

  protected onPointerCancel = () => {
    // Pointer cancelled, so cancel any pending gestures.
    this.handleGestureCancel();

    this.currentGesture = emptyGestureEvent();
    this.draggingRespondent = DragFeature.NONE;
    this.removeDragListeners();
    this.resetCursor();
  };

  protected handleGestureStart() {
    if (!this.currentGesture) {
      return;
    }
    this.currentGesture = {
      ...this.currentGesture,
      state: GestureState.STARTING,
    };

    // Send events to hide UI.
    this.baseHandler.closePreselectionBubble();
    this.fire('selection-started');

    if (this.enableBorderGlow) {
      this.getOverlayBorderGlow().handleGestureStart();

      // If there is no post selection, fade the scrim from the region selection
      // back in.
      if (!this.selectionElements.postSelectionRenderer.hasSelection()) {
        // TODO(crbug.com/421002691): follow the convention where the layer
        // should return true if its handling the gesture, and
        // draggingRespondent should be updated. Currently used to trigger the
        // fade in of the darkened scrim.
        this.selectionElements.regionSelectionLayer.handleGestureStart();
      }
    }
  }

  protected handleGestureDrag(event: PointerEvent) {
    if (!this.currentGesture) {
      return;
    }
    assert(this.currentGesture.state === GestureState.DRAGGING);
    // Capture pointer events so gestures still work if the users pointer
    // leaves the selection overlay div. Pointer capture is implicitly
    // released after pointerup or pointercancel events.
    this.setPointerCapture(event.pointerId);
  }

  protected handleGestureEnd() {}

  protected handleGestureCancel() {
    this.selectionElements.regionSelectionLayer.cancelGesture();
    this.selectionElements.postSelectionRenderer.cancelGesture();
  }

  private handlePostSelectionUpdated(height: number, width: number) {
    const overlayBorderGlow = this.getOverlayBorderGlow();
    // If there is no selection happening, fade the glow back in.
    if (width === 0 && height === 0 &&
        this.draggingRespondent === DragFeature.NONE) {
      overlayBorderGlow.handleClearSelection();
      this.selectionElements.regionSelectionLayer.handlePostSelectionCleared();
      return;
    }

    overlayBorderGlow.handlePostSelectionUpdated();
  }

  private updateCanvasSize(containerWidth: number, containerHeight: number) {
    // Set our own canvas size while preserving the canvas aspect ratio.
    const screenshotHeight =
        this.selectionElements.backgroundImageCanvas.height;
    const screenshotWidth = this.selectionElements.backgroundImageCanvas.width;

    const doesScreenshotFillContainer =
        Math.abs(
            containerWidth - (screenshotWidth / window.devicePixelRatio)) <=
            SCREENSHOT_RESIZE_TOLERANCE_PIXELS &&
        Math.abs(
            containerHeight - (screenshotHeight / window.devicePixelRatio)) <=
            SCREENSHOT_RESIZE_TOLERANCE_PIXELS;
    const shouldApplyMargins =
        !doesScreenshotFillContainer || this.sidePanelOpened;

    // Apply margins if the page is resized / side panel is opened and not
    // closing.
    const margins = shouldApplyMargins && !this.isClosing ?
        SCREENSHOT_FULLSIZE_MARGIN_PIXEL * 2 :
        0;
    const newContainerWidth = containerWidth - margins;
    const newContainerHeight = containerHeight - margins;

    // Get the aspect ratio to force the image to conform to.
    const aspectRatio = screenshotWidth / screenshotHeight;

    // Calculate potential dimensions based on width and height
    const widthBasedHeight = Math.round(newContainerWidth / aspectRatio);
    const heightBasedWidth = Math.round(newContainerHeight * aspectRatio);

    // Choose dimensions that fit within the container while preserving aspect
    // ratio
    if (widthBasedHeight <= newContainerHeight) {
      // Width-based dimensions fit
      this.canvasHeight = widthBasedHeight;
      this.canvasWidth = newContainerWidth;
    } else {
      // Height-based dimensions fit
      this.canvasWidth = heightBasedWidth;
      this.canvasHeight = newContainerHeight;
    }

    this.isResized = shouldApplyMargins;
    if (this.isResized) {
      this.isInitialSize = false;
      // The flash animation is cut short but animationend is never called if
      // the overlay is resized before animationend is called. This is because
      // the flash scrim is hidden on resize.
      this.onInitialFlashAnimationEnd();
    }
  }

  private handleResize(entries: ResizeObserverEntry[]) {
    // Cancel a pending event to prevent multiple updates per frame.
    if (this.handleResizeRequestId) {
      cancelAnimationFrame(this.handleResizeRequestId);
    }

    // Use requestAnimationFrame to only calculate the screenshot size once
    // a frame instead of multiple times per frame.
    this.handleResizeRequestId = requestAnimationFrame(async () => {
      assert(entries.length === 1);
      const newRect = entries[0].contentRect;

      // If the screenshot is not rendered yet, there is nothing to do yet.
      if (!this.isScreenshotRendered ||
          (newRect.width === 0 && newRect.height === 0)) {
        this.handleResizeRequestId = undefined;
        return;
      }

      this.updateCanvasSize(newRect.width, newRect.height);

      // Wait for the canvas size update to be rendered so that we can calculate
      // the selection overlay rect.
      await this.updateComplete;

      // Update our cached selection overlay rect to the new bounds.
      this.updateSelectionOverlayRect();

      this.handleResizeRequestId = undefined;
    });
  }

  protected updateSelectionOverlayRect(): void {
    // We use getBoundingClientRect() instead of offsetXXX because offsetXXX is
    // relative to the offsetParent, which might not be the viewport. clientX
    // and clientY are viewport-relative, so we need viewport-relative bounds
    // to calculate relative coordinates.
    const selectionOverlay = this.selectionElements.selectionOverlay;
    if (!selectionOverlay) {
      return;
    }
    const rect = selectionOverlay.getBoundingClientRect();
    this.selectionOverlayRect =
        new DOMRect(rect.left, rect.top, rect.width, rect.height);

    // If dimensions are zero, retry in next frame as layout might still be
    // occurring.
    if (this.selectionOverlayRect.width === 0 ||
        this.selectionOverlayRect.height === 0) {
      requestAnimationFrame(this.updateSelectionOverlayRect.bind(this));
    } else {
      this.resizeSelectionCanvases(
          this.selectionOverlayRect.width, this.selectionOverlayRect.height);
    }
  }

  protected resizeSelectionCanvases(newWidth: number, newHeight: number) {
    this.selectionElements.regionSelectionLayer.setCanvasSizeTo(
        newWidth, newHeight);
    this.selectionElements.postSelectionRenderer.setCanvasSizeTo(
        newWidth, newHeight);
    this.selectionElements.overlayShimmerCanvas.setCanvasSizeTo(
        newWidth, newHeight);
  }

  // Updates the currentGesture to correspond with the given PointerEvent.
  private updateGestureCoordinates(event: PointerEvent) {
    if (!this.currentGesture) {
      return;
    }
    this.currentGesture = {
      ...this.currentGesture,
      clientX: event.clientX,
      clientY: event.clientY,
    };
  }

  // Returns if the given PointerEvent should be ignored.
  protected shouldIgnoreEvent(event: PointerEvent) {
    if (this.isClosing) {
      return true;
    }
    // Ignore multi touch events and non-left/right click events.
    return !event.isPrimary || (event.button !== 0 && event.button !== 2);
  }

  // Returns whether the current gesture event is a drag.
  private isDragging() {
    if (!this.currentGesture) {
      return false;
    }
    if (this.currentGesture.state === GestureState.DRAGGING) {
      return true;
    }

    // TODO(b/329514345): Revisit if pointer movement is enough of an indicator,
    // or if we also need a timelimit on how long a tap can last before starting
    // a drag.
    const xMovement =
        Math.abs(this.currentGesture.clientX - this.currentGesture.startX);
    const yMovement =
        Math.abs(this.currentGesture.clientY - this.currentGesture.startY);
    return xMovement > DRAG_THRESHOLD || yMovement > DRAG_THRESHOLD;
  }

  private onInitialFlashAnimationEnd() {
    if (this.hasInitialFlashAnimationEnded) {
      return;
    }
    this.hasInitialFlashAnimationEnded = true;
    this.eventTracker_.remove(
        this.selectionElements.initialFlashScrim, 'animationend');

    this.baseHandler.addBackgroundBlur();

    // Let the parent know the initial flash image animation has finished.
    this.fire('initial-flash-animation-end');

    // Don't start the shimmer animation until the initial flash animation is
    // finished.
    if (!this.disableShimmer && !this.enableBorderGlow) {
      this.selectionElements.overlayShimmerCanvas.startAnimation();
    }
  }

  private async screenshotDataReceived(
      screenshotBitmap: ImageBitmap, isSidePanelOpen: boolean) {
    await this.updateComplete;
    renderScreenshot(
        this.selectionElements.backgroundImageCanvas, screenshotBitmap);
    // Start the canvas as the same dimensions as the viewport, since we are
    // assuming the screenshot takes up the viewport dimensions. Our resize
    // handler will adjust as needed.
    this.canvasWidth = window.innerWidth;
    this.canvasHeight = window.innerHeight;

    // This is the first time the screenshot has been rendered.
    this.isScreenshotRendered = true;
    if (isSidePanelOpen) {
      this.setSidePanelOpened();
    }

    // Wait for the canvas size update to be rendered so that we can calculate
    // the selection overlay rect.
    await this.updateComplete;
    this.updateSelectionOverlayRect();

    this.onImageRendered();
  }

  protected setSidePanelOpened() {
    // Reset the state of the selection overlay to represent the overlay being
    // opened with the side panel open.
    this.sidePanelOpened = true;
    this.isResized = true;
    this.isInitialSize = false;
  }

  private async onOverlayReshown(screenshotBitmap: ImageBitmap) {
    await this.updateComplete;
    // Render the new screenshot.
    renderScreenshot(
        this.selectionElements.backgroundImageCanvas, screenshotBitmap);

    // Reset the state of the selection overlay to represent the overlay being
    // opened with the side panel open.
    this.isClosing = false;
    this.sidePanelOpened = true;
    this.hideBackgroundImageCanvas = true;

    this.updateCanvasSize(window.innerWidth, window.innerHeight);

    // Update our cached selection overlay rect to the new bounds.
    await this.updateComplete;
    this.updateSelectionOverlayRect();

    // Allow the new screenshot to render / allow any resizing that needs to
    // happen before finishing the reshow overlay flow. This needs an extra
    // animation frame after the next render to ensure the new screenshot is
    // painted at least once.
    requestAnimationFrame(() => {
      this.onFinishReshowOverlay();
    });
  }

  abstract getOverlayBorderGlow(): OverlayBorderGlowElement;

  protected onFinishReshowOverlay() {
    this.hideBackgroundImageCanvas = false;
    this.fire('on-finish-reshow-overlay');
  }

  /**
   * Returns the bounding rect of the selection overlay. This is preferred over
   * using getBoundingClientRect() because it is a cached DOM property which
   * doesn't need to be recalculated every time.
   */
  getBoundingRect() {
    return this.selectionOverlayRect;
  }

  fetchNewScreenshotForTesting() {
    ScreenshotBitmapBrowserProxyImpl.getInstance().fetchScreenshot(
        this.screenshotDataReceived.bind(this));
  }

  getHideBackgroundImageCanvasForTesting() {
    return this.hideBackgroundImageCanvas;
  }
}
