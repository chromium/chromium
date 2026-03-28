// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './region_selection.js';
import './post_selection_renderer.js';
import './overlay_border_glow.js';
import './overlay_shimmer_canvas.js';
import '/strings.m.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getFallbackTheme} from './color_utils.js';
import type {OverlayTheme} from './lens.mojom-webui.js';
import type {OverlayBorderGlowElement} from './overlay_border_glow.js';
import type {OverlayShimmerCanvasElement} from './overlay_shimmer_canvas.js';
import type {PostSelectionBoundingBox, PostSelectionRendererElement} from './post_selection_renderer.js';
import type {RegionSelectionElement} from './region_selection.js';
import {ScreenshotBitmapBrowserProxyImpl} from './screenshot_bitmap_browser_proxy.js';
import {renderScreenshot} from './screenshot_utils.js';
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

const SelectionOverlayElementBase = I18nMixin(PolymerElement);

/*
 * Element responsible for coordinating selections between the various selection
 * features. This includes:
 *   - Storing state needed to coordinate selections across features
 *   - Listening to mouse/tap events and delegating them to the correct features
 *   - Coordinating animations between the different features
 */
export abstract class SelectionOverlayBaseElement extends
    SelectionOverlayElementBase {
  protected abstract backgroundImageCanvas(): HTMLCanvasElement;
  protected abstract cursor(): HTMLElement;
  protected abstract initialFlashScrim(): HTMLElement;
  protected abstract overlayShimmerCanvas(): OverlayShimmerCanvasElement;
  protected abstract postSelectionRenderer(): PostSelectionRendererElement;
  protected abstract regionSelectionLayer(): RegionSelectionElement;
  protected abstract selectionOverlay(): HTMLElement;

  static get properties(): PolymerElementProperties {
    return {
      isScreenshotRendered: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      isResized: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
        observer: 'onIsResizedChanged',
      },
      isInitialSize: {
        type: Boolean,
        reflectToAttribute: true,
        value: true,
      },
      canvasHeight: Number,
      canvasWidth: Number,
      isPointerInside: {
        type: Boolean,
        value: false,
      },
      currentGesture: {
        type: Object,
        value: () => emptyGestureEvent(),
      },
      disableShimmer: {
        type: Boolean,
        readOnly: true,
        value: !loadTimeData.getBoolean('enableShimmer'),
      },
      enableBorderGlow: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableBorderGlow'),
      },
      isClosing: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      shimmerOnSegmentation: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      shimmerFadeOutComplete: {
        type: Boolean,
        reflectToAttribute: true,
        value: true,
      },
      darkenExtraScrim: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      theme: {
        type: Object,
        value: getFallbackTheme,
      },
      selectionOverlayRect: Object,
      hideBackgroundImageCanvas: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      sidePanelOpened: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      activeRegionId: {
        type: String,
        value: '',
        reflectToAttribute: true,
      },
    };
  }

  // Whether the screenshot has finished loading in.
  declare private isScreenshotRendered: boolean;
  // Whether the selection overlay is its initial size, or has changed size.
  declare private isResized: boolean;
  declare private isInitialSize: boolean;
  // Width and height values for rendering the background image canvas as the
  // proper dimensions.
  declare protected canvasHeight: number;
  declare protected canvasWidth: number;
  // The current content rectangle of the selection elements DIV. This is the
  // bounds of the screenshot and the part the user interacts with. This should
  // be used instead of call getBoundingClientRect().
  declare private selectionOverlayRect: DOMRect;

  // The current gesture event. The coordinate values are only accurate if a
  // gesture has started.
  declare protected currentGesture: GestureEvent;
  declare private disableShimmer: boolean;
  declare private enableBorderGlow: boolean;
  // Whether the overlay is being shut down.
  declare private isClosing: boolean;
  // Whether the default background scrim is currently being darkened.
  declare private darkenExtraScrim: boolean;
  // Whether the shimmer is currently focused on a segmentation mask.
  declare private shimmerOnSegmentation: boolean;
  declare private shimmerFadeOutComplete: boolean;
  // Whether the side panel is currently opened.
  declare protected sidePanelOpened: boolean;
  // Whether the background image canvas should currently be shown.
  declare private hideBackgroundImageCanvas: boolean;
  declare protected isPointerInside: boolean;

  // The border glow layer rendered on the selection overlay if it exists.
  private overlayBorderGlow: OverlayBorderGlowElement;

  protected eventTracker_: EventTracker = new EventTracker();
  // Listener ids for events from the browser side.
  protected listenerIds: number[];
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
  declare protected activeRegionId: string;

  // The ID returned by requestAnimationFrame for the updateCursorPosition,
  // onPointerMove, and handleResize functions.
  private updateCursorPositionRequestId?: number;
  private onPointerMoveRequestId?: number;
  private handleResizeRequestId?: number;

  declare private theme: OverlayTheme;

  override connectedCallback() {
    super.connectedCallback();
    this.resizeObserver.observe(this);
    this.listenerIds = [
      this.baseHandler.addNotifyOverlayClosingListener(() => {
        this.isClosing = true;
        this.removeDragListeners();
      }),
      this.baseHandler.addMultiRegionSelectionListener((regions) => {
        if (regions.length > 0 && !this.activeRegionId) {
          this.activeRegionId = regions[0].id;
        }
      }),
    ];
    ScreenshotBitmapBrowserProxyImpl.getInstance().fetchScreenshot(
        this.screenshotDataReceived.bind(this));
    ScreenshotBitmapBrowserProxyImpl.getInstance().addOnOverlayReshownListener(
        this.onOverlayReshown.bind(this));
    this.eventTracker_.add(
        document, 'shimmer-fade-out-complete', (e: CustomEvent<boolean>) => {
          this.shimmerFadeOutComplete = e.detail;
        });
    this.eventTracker_.add(
        document, 'set-cursor', (e: CustomEvent<CursorData>) => {
          if (e.detail.cursor === CursorType.POINTER) {
            this.setCursorToPointer();
          } else if (e.detail.cursor === CursorType.CROSSHAIR) {
            this.setCursorToCrosshair();
          } else if (e.detail.cursor === CursorType.TEXT) {
            this.setCursorToText();
          } else {
            this.resetCursor();
          }
        });
    this.eventTracker_.add(document, 'darken-extra-scrim-opacity', () => {
      this.darkenExtraScrim = true;
    });
    this.eventTracker_.add(document, 'lighten-extra-scrim-opacity', () => {
      this.darkenExtraScrim = false;
    });
    this.eventTracker_.add(
        this.initialFlashScrim(), 'animationend', (event: AnimationEvent) => {
          // The flash animation is the longest animation.
          if (event.animationName !== 'initial-inset-animation') {
            return;
          }
          this.onInitialFlashAnimationEnd();
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

    assert(this.matchMedia);
    this.matchMedia.removeEventListener(
        'change', this.onDevicePixelRatioChanged.bind(this));
  }

  override ready() {
    super.ready();
    this.addEventListener('pointerdown', this.onPointerDown.bind(this));
    this.addEventListener('pointermove', this.updateCursorPosition.bind(this));
  }

  protected onActivateRegion(event: CustomEvent<{id: string}>) {
    this.activeRegionId = event.detail.id;
    event.stopPropagation();
  }

  private addDragListeners() {
    this.addEventListener('pointerup', this.onPointerUp);
    this.addEventListener('pointermove', this.onPointerMove);
    this.addEventListener('pointercancel', this.onPointerCancel);
  }

  private removeDragListeners() {
    this.removeEventListener('pointerup', this.onPointerUp);
    this.removeEventListener('pointermove', this.onPointerMove);
    this.removeEventListener('pointercancel', this.onPointerCancel);
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

      if (!this.disableShimmer &&
          (this.isPointerInside ||
           this.currentGesture.state === GestureState.DRAGGING)) {
        this.updateShimmerForCursor(cursorOffsetX, cursorOffsetY);
      }

      this.cursor().style.transform =
          `translate3d(${cursorOffsetX}px, ${cursorOffsetY}px, 0)`;
      this.updateCursorPositionRequestId = undefined;
    });
  }

  private updateShimmerForCursor(cursorLeft: number, cursorTop: number) {
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

  private getHiddenCursorClass(isPointerInside: boolean, state: GestureState):
      string {
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
    this.style.setProperty(CURSOR_IMG_URL, 'url("lens.svg")');
  }

  // Called on object hover.
  protected setCursorToPointer() {
    // No dragging for objects, so no need to set body cursor style.
    this.cursorOffsetX = 11;
    this.cursorOffsetY = 17;
    this.style.setProperty(CURSOR_IMG_URL, 'url("lens.svg")');
  }

  protected resetCursor() {
    document.body.style.cursor = 'unset';
    this.cursorOffsetX = 3;
    this.cursorOffsetY = 6;
    this.style.setProperty(CURSOR_IMG_URL, 'url("lens.svg")');
  }
  // LINT.ThenChange(//chrome/browser/resources/lens/overlay/cursor_tooltip.ts:CursorOffsetValues)


  private onImageRendered() {
    // Let the parent know it is safe to blur the background.
    this.dispatchEvent(new CustomEvent('screenshot-rendered', {
      bubbles: true,
      composed: true,
      detail: {isSidePanelOpen: this.sidePanelOpened},
    }));
    this.baseHandler.notifyOverlayInitialized();
  }

  protected handlePointerEnter() {
    this.isPointerInside = true;
  }

  private handlePointerLeave() {
    this.isPointerInside = false;
  }

  private onPointerDown(event: PointerEvent) {
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
    this.postSelectionRenderer().handleRightClick(event);
  }

  private onPointerUp(event: PointerEvent) {
    this.updateGestureCoordinates(event);

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
  }

  private onPointerMove(event: PointerEvent) {
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
      this.set('currentGesture.state', GestureState.DRAGGING);
    }

    // If we haven't exited early, we must be in the dragging state.
    assert(this.currentGesture.state === GestureState.DRAGGING);

    // Handle the drag.
    this.cancelPendingDragAnimationFrame();
    this.onPointerMoveRequestId = requestAnimationFrame(() => {
      this.handleGestureDrag(event);
      this.onPointerMoveRequestId = undefined;
    });
  }

  private cancelPendingDragAnimationFrame() {
    if (this.onPointerMoveRequestId) {
      cancelAnimationFrame(this.onPointerMoveRequestId);
    }
  }

  private onPointerCancel() {
    // Pointer cancelled, so cancel any pending gestures.
    this.handleGestureCancel();

    this.currentGesture = emptyGestureEvent();
    this.draggingRespondent = DragFeature.NONE;
    this.removeDragListeners();
    this.resetCursor();
  }

  protected handleGestureStart() {
    this.set('currentGesture.state', GestureState.STARTING);

    // Send events to hide UI.
    this.baseHandler.closePreselectionBubble();
    this.dispatchEvent(
        new CustomEvent('selection-started', {bubbles: true, composed: true}));

    if (this.enableBorderGlow) {
      this.getOverlayBorderGlow().handleGestureStart();

      // If there is no post selection, fade the scrim from the region selection
      // back in.
      if (!this.postSelectionRenderer().hasSelection()) {
        // TODO(crbug.com/421002691): follow the convention where the layer
        // should return true if its handling the gesture, and
        // draggingRespondent should be updated. Currently used to trigger the
        // fade in of the darkened scrim.
        this.regionSelectionLayer().handleGestureStart();
      }
    }
  }

  protected handleGestureDrag(event: PointerEvent) {
    assert(this.currentGesture.state === GestureState.DRAGGING);
    // Capture pointer events so gestures still work if the users pointer
    // leaves the selection overlay div. Pointer capture is implicitly
    // released after pointerup or pointercancel events.
    this.setPointerCapture(event.pointerId);
  }

  protected handleGestureEnd() {}

  protected handleGestureCancel() {
    this.regionSelectionLayer().cancelGesture();
    this.postSelectionRenderer().cancelGesture();
  }

  private handlePostSelectionUpdated(height: number, width: number) {
    const overlayBorderGlow = this.getOverlayBorderGlow();
    // If there is no selection happening, fade the glow back in.
    if (width === 0 && height === 0 &&
        this.draggingRespondent === DragFeature.NONE) {
      overlayBorderGlow.handleClearSelection();
      this.regionSelectionLayer().handlePostSelectionCleared();
      return;
    }

    overlayBorderGlow.handlePostSelectionUpdated();
  }

  private updateCanvasSize(containerWidth: number, containerHeight: number) {
    // Set our own canvas size while preserving the canvas aspect ratio.
    const screenshotHeight = this.backgroundImageCanvas().height;
    const screenshotWidth = this.backgroundImageCanvas().width;

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
    const aspectRatio = this.backgroundImageCanvas().width /
        this.backgroundImageCanvas().height;

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
    this.handleResizeRequestId = requestAnimationFrame(() => {
      assert(entries.length === 1);
      const newRect = entries[0].contentRect;

      // If the screenshot is not rendered yet, there is nothing to do yet.
      if (!this.isScreenshotRendered ||
          (newRect.width === 0 && newRect.height === 0)) {
        this.handleResizeRequestId = undefined;
        return;
      }

      this.updateCanvasSize(newRect.width, newRect.height);

      // Update our cached selection overlay rect to the new bounds.
      this.updateSelectionOverlayRect();

      // TODO(b/361798599): Since we now pass selectionOverlayRect, we can use
      // polymer events to allow each client to resize their canvas once
      // selectionOverlayRect changes. We should remove this and do the
      // resizing via polymer techniques.
      this.resizeSelectionCanvases(
          this.selectionOverlayRect.width, this.selectionOverlayRect.height);

      this.handleResizeRequestId = undefined;
    });
  }

  private updateSelectionOverlayRect(): void {
    // We use offsetXXX instead of call this.getBoundingClientRect() because
    // offsetXXX is a cached DOM property, while this.getBoundingClientRect()
    // recalculates the layout every time it is called. Since we have no
    // scrolling, these calls should be equivalent.
    this.selectionOverlayRect = new DOMRect(
        this.selectionOverlay().offsetLeft, this.selectionOverlay().offsetTop,
        this.selectionOverlay().offsetWidth,
        this.selectionOverlay().offsetHeight);
  }

  protected resizeSelectionCanvases(newWidth: number, newHeight: number) {
    this.regionSelectionLayer().setCanvasSizeTo(newWidth, newHeight);
    this.postSelectionRenderer().setCanvasSizeTo(newWidth, newHeight);
    this.overlayShimmerCanvas().setCanvasSizeTo(newWidth, newHeight);
  }

  // Updates the currentGesture to correspond with the given PointerEvent.
  private updateGestureCoordinates(event: PointerEvent) {
    this.currentGesture.clientX = event.clientX;
    this.currentGesture.clientY = event.clientY;
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
    this.eventTracker_.remove(this.initialFlashScrim(), 'animationend');

    this.baseHandler.addBackgroundBlur();

    // Let the parent know the initial flash image animation has finished.
    this.dispatchEvent(new CustomEvent(
        'initial-flash-animation-end', {bubbles: true, composed: true}));

    // Don't start the shimmer animation until the initial flash animation is
    // finished.
    if (!this.disableShimmer && !this.enableBorderGlow) {
      this.overlayShimmerCanvas().startAnimation();
    }
  }

  private screenshotDataReceived(
      screenshotBitmap: ImageBitmap, isSidePanelOpen: boolean) {
    renderScreenshot(this.backgroundImageCanvas(), screenshotBitmap);
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
    this.onImageRendered();
  }

  protected setSidePanelOpened() {
    // Reset the state of the selection overlay to represent the overlay being
    // opened with the side panel open.
    this.sidePanelOpened = true;
    this.isResized = true;
    this.isInitialSize = false;
  }

  private onOverlayReshown(screenshotBitmap: ImageBitmap) {
    // Render the new screenshot.
    renderScreenshot(this.backgroundImageCanvas(), screenshotBitmap);

    // Reset the state of the selection overlay to represent the overlay being
    // opened with the side panel open.
    this.isClosing = false;
    this.sidePanelOpened = true;
    this.hideBackgroundImageCanvas = true;

    this.updateCanvasSize(window.innerWidth, window.innerHeight);

    // Update our cached selection overlay rect to the new bounds.
    this.updateSelectionOverlayRect();
    this.resizeSelectionCanvases(
        this.selectionOverlayRect.width, this.selectionOverlayRect.height);

    // Allow the new screenshot to render / allow any resizing that needs to
    // happen before finishing the reshow overlay flow. This needs an extra
    // animation frame after the next render to ensure the new screenshot is
    // painted at least once.
    afterNextRender(this.backgroundImageCanvas(), () => {
      requestAnimationFrame(() => {
        this.onFinishReshowOverlay();
      });
    });
  }

  private getOverlayBorderGlow(): OverlayBorderGlowElement {
    if (this.overlayBorderGlow) {
      return this.overlayBorderGlow;
    }
    this.overlayBorderGlow =
        this.shadowRoot!.querySelector('overlay-border-glow')!;
    return this.overlayBorderGlow;
  }

  protected onFinishReshowOverlay() {
    this.hideBackgroundImageCanvas = false;
    this.dispatchEvent(new CustomEvent(
        'on-finish-reshow-overlay', {bubbles: true, composed: true}));
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
