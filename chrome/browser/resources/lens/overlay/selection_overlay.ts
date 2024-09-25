// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './object_layer.js';
import './text_layer.js';
import './region_selection.js';
import './post_selection_renderer.js';
import './overlay_shimmer.js';
import './overlay_shimmer_canvas.js';
import './strings.m.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getFallbackTheme} from './color_utils.js';
import {type CursorTooltipData, CursorTooltipType} from './cursor_tooltip.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import {UserAction} from './lens.mojom-webui.js';
import {INVOCATION_SOURCE} from './lens_overlay_app.js';
import {recordLensOverlayInteraction} from './metrics_utils.js';
import type {ObjectLayerElement} from './object_layer.js';
import type {OverlayShimmerElement} from './overlay_shimmer.js';
import type {OverlayShimmerCanvasElement} from './overlay_shimmer_canvas.js';
import type {PostSelectionRendererElement} from './post_selection_renderer.js';
import type {RegionSelectionElement} from './region_selection.js';
import {ScreenshotBitmapBrowserProxyImpl} from './screenshot_bitmap_browser_proxy.js';
import {renderScreenshot} from './screenshot_utils.js';
import {getTemplate} from './selection_overlay.html.js';
import {CursorType, DRAG_THRESHOLD, DragFeature, emptyGestureEvent, focusShimmerOnRegion, GestureState, ShimmerControlRequester, unfocusShimmer} from './selection_utils.js';
import type {GestureEvent, OverlayShimmerFocusedRegion} from './selection_utils.js';
import type {TextLayerElement} from './text_layer.js';
import type {TranslateState} from './translate_button.js';
import {toPercent} from './values_converter.js';

// The amount of margins in pixels to add to the screenshot when the window is
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

export interface SelectedTextContextMenuData {
  // The text selection that the context menu commands will act on.
  text: string;
  // Dominant content language of the text. Language code is CLDR/BCP-47.
  contentLanguage: string;
  // The left-most position of the selected text.
  left: number;
  // The right-most position of the selected text.
  right: number;
  // The highest position of the selected text.
  top: number;
  // The lowest position of the selected text.
  bottom: number;
  // The selection start index of the text.
  selectionStartIndex: number;
  // The end selection index of the text.
  selectionEndIndex: number;
}

export interface SelectedRegionContextMenuData {
  // The bounds of the selected region.
  box: CenterRotatedBox;
  // The selection start index of the detected text, or -1 if none.
  selectionStartIndex: number;
  // The end selection index of the detected text, or -1 if none.
  selectionEndIndex: number;
}

export interface SelectionOverlayElement {
  $: {
    backgroundImageCanvas: HTMLCanvasElement,
    cursor: HTMLElement,
    initialFlashScrim: HTMLDivElement,
    objectSelectionLayer: ObjectLayerElement,
    overlayShimmerCanvas: OverlayShimmerCanvasElement,
    overlayShimmer: OverlayShimmerElement,
    postSelectionRenderer: PostSelectionRendererElement,
    regionSelectionLayer: RegionSelectionElement,
    selectedRegionContextMenu: HTMLElement,
    selectedTextContextMenu: HTMLElement,
    selectionOverlay: HTMLElement,
    textSelectionLayer: TextLayerElement,
  };
}

const SelectionOverlayElementBase = I18nMixin(PolymerElement);

/*
 * Element responsible for coordinating selections between the various selection
 * features. This includes:
 *   - Storing state needed to coordinate selections across features
 *   - Listening to mouse/tap events and delegating them to the correct features
 *   - Coordinating animations between the different features
 */
export class SelectionOverlayElement extends SelectionOverlayElementBase {
  static get is() {
    return 'lens-selection-overlay';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isScreenshotRendered: {
        type: Boolean,
        reflectToAttribute: true,
      },
      isResized: {
        type: Boolean,
        reflectToAttribute: true,
      },
      isInitialSize: {
        type: Boolean,
        reflectToAttribute: true,
      },
      showTranslateContextMenuItem: {
        type: Boolean,
        reflectToAttribute: true,
      },
      showSelectedTextContextMenu: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      showSelectedRegionContextMenu: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      showDetectedTextContextMenuOptions: {
        type: Boolean,
        reflectToAttribute: true,
      },
      selectedTextContextMenuX: Number,
      selectedTextContextMenuY: Number,
      selectedRegionContextMenuX: Number,
      selectedRegionContextMenuY: Number,
      canvasHeight: Number,
      canvasWidth: Number,
      isPointerInside: Boolean,
      currentGesture: emptyGestureEvent(),
      disableShimmer: {
        type: Boolean,
        readOnly: true,
        value: !loadTimeData.getBoolean('enableShimmer'),
      },
      useShimmerCanvas: {
        type: Boolean,
        readOnly: true,
        value: loadTimeData.getBoolean('useShimmerCanvas'),
      },
      enableCopyAsImage: {
        type: Boolean,
        reflectToAttribute: true,
      },
      enableSaveAsImage: {
        type: Boolean,
        reflectToAttribute: true,
      },
      isClosing: {
        type: Boolean,
        reflectToAttribute: true,
      },
      suppressCopyAndSaveAsImage: {
        type: Boolean,
        reflectToAttribute: true,
      },
      shimmerOnSegmentation: {
        type: Boolean,
        reflectToAttribute: true,
      },
      shimmerFadeOutComplete: {
        type: Boolean,
        reflectToAttribute: true,
      },
      darkenExtraScrim: {
        type: Boolean,
        reflectToAttribute: true,
      },
      theme: {
        type: Object,
        value: getFallbackTheme,
      },
      translateModeEnabled: {
        type: Boolean,
        reflectToAttribute: true,
      },
      selectionOverlayRect: Object,
      isSearchboxFocused: Boolean,
    };
  }

  // Whether the screenshot has finished loading in.
  private isScreenshotRendered: boolean = false;
  // Whether the selection overlay is its initial size, or has changed size.
  private isResized: boolean = false;
  private isInitialSize: boolean = true;
  private showTranslateContextMenuItem: boolean = true;
  private showSelectedTextContextMenu: boolean;
  private showSelectedRegionContextMenu: boolean;
  private showDetectedTextContextMenuOptions: boolean;
  // Location at which to show the context menus.
  private selectedTextContextMenuX: number;
  private selectedTextContextMenuY: number;
  private selectedRegionContextMenuX: number;
  private selectedRegionContextMenuY: number;
  // Width and height values for rendering the background image canvas as the
  // proper dimensions.
  private canvasHeight: number;
  private canvasWidth: number;
  // The current content rectangle of the selection elements DIV. This is the
  // bounds of the screenshot and the part the user interacts with. This should
  // be used instead of call getBoundingClientRect().
  private selectionOverlayRect: DOMRect;
  // Whether the users focus is currently in the overlay searchbox. Passed in
  // from parent.
  private isSearchboxFocused: boolean;

  // The selected region on which the context menu is being displayed. Used as
  // argument for copy and save as image calls.
  private selectedRegionContextMenuBox: CenterRotatedBox;
  private highlightedText: string = '';
  private contentLanguage: string = '';
  private textSelectionStartIndex: number = -1;
  private textSelectionEndIndex: number = -1;
  private detectedTextStartIndex: number = -1;
  private detectedTextEndIndex: number = -1;
  private isPointerInside = false;
  private isPointerInsideContextMenu = false;
  // The current gesture event. The coordinate values are only accurate if a
  // gesture has started.
  private currentGesture: GestureEvent = emptyGestureEvent();
  private disableShimmer: boolean;
  private useShimmerCanvas: boolean;
  private enableCopyAsImage: boolean =
      loadTimeData.getBoolean('enableCopyAsImage');
  private enableSaveAsImage: boolean =
      loadTimeData.getBoolean('enableSaveAsImage');
  private suppressCopyAndSaveAsImage: boolean =
      loadTimeData.getString('invocationSource') ===
      'ContentAreaContextMenuImage';
  // Whether the overlay is being shut down.
  private isClosing: boolean = false;
  // Whether the default background scrim is currently being darkened.
  private darkenExtraScrim: boolean = false;
  // Whether the shimmer is currently focused on a segmentation mask.
  private shimmerOnSegmentation: boolean = false;
  private shimmerFadeOutComplete: boolean = true;

  private eventTracker_: EventTracker = new EventTracker();
  // Listener ids for events from the browser side.
  private listenerIds: number[];
  // The feature currently being dragged. Once a feature responds to a drag
  // event, no other feature will receive gesture events.
  private draggingRespondent = DragFeature.NONE;
  private resizeObserver: ResizeObserver =
      new ResizeObserver(this.handleResize.bind(this));
  // Used to listen for changes in the window.devicePixelRatio. Stored as a
  // variable so we can easily add and remove the listener.
  private matchMedia?: MediaQueryList;
  private cursorOffsetX: number = 3;
  private cursorOffsetY: number = 6;
  private hasInitialFlashAnimationEnded = false;
  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();

  // The ID returned by requestAnimationFrame for the updateCursorPosition,
  // onPointerMove, and handleResize functions.
  private updateCursorPositionRequestId?: number;
  private onPointerMoveRequestId?: number;
  private handleResizeRequestId?: number;

  // Whether or not translate mode is enabled. If true, only text should
  // be selectable, and it should be selectable from any point in the
  // overlay.
  private translateModeEnabled: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.resizeObserver.observe(this);
    this.listenerIds = [
      this.browserProxy.callbackRouter.notifyOverlayClosing.addListener(() => {
        this.isClosing = true;
        this.removeDragListeners();
      }),
      this.browserProxy.callbackRouter.triggerCopyText.addListener(() => {
        this.handleCopy();
      }),
    ];
    ScreenshotBitmapBrowserProxyImpl.getInstance().fetchScreenshot(
        this.screenshotDataReceived.bind(this));
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
    this.eventTracker_.add(
        document, 'translate-mode-state-changed',
        (e: CustomEvent<TranslateState>) => {
          this.showTranslateContextMenuItem = !e.detail.translateModeEnabled;
          this.translateModeEnabled = e.detail.translateModeEnabled;
          // Resetting the cursor will properly set it to text or normal
          // based on the current translate mode.
          this.resetCursor();
        });
    this.eventTracker_.add(
        document, 'show-selected-text-context-menu',
        (e: CustomEvent<SelectedTextContextMenuData>) => {
          this.showSelectedTextContextMenu = true;
          this.selectedTextContextMenuX = e.detail.left;
          this.selectedTextContextMenuY = e.detail.bottom;
          this.highlightedText = e.detail.text;
          this.contentLanguage = e.detail.contentLanguage;
          this.textSelectionStartIndex = e.detail.selectionStartIndex;
          this.textSelectionEndIndex = e.detail.selectionEndIndex;
        });
    this.eventTracker_.add(
        document, 'restore-selected-text-context-menu', () => {
          // show-selected-text-context-menu or
          // update-selected-text-context-menu must be triggered first so that
          // instance variables are set.
          this.showSelectedTextContextMenu = true;
        });
    this.eventTracker_.add(
        document, 'update-selected-text-context-menu',
        (e: CustomEvent<SelectedTextContextMenuData>) => {
          this.selectedTextContextMenuX = e.detail.left;
          this.selectedTextContextMenuY = e.detail.bottom;
          this.highlightedText = e.detail.text;
          this.contentLanguage = e.detail.contentLanguage;
          this.textSelectionStartIndex = e.detail.selectionStartIndex;
          this.textSelectionEndIndex = e.detail.selectionEndIndex;
        });
    this.eventTracker_.add(document, 'hide-selected-text-context-menu', () => {
      this.showSelectedTextContextMenu = false;
      this.textSelectionStartIndex = -1;
      this.textSelectionEndIndex = -1;
    });
    this.eventTracker_.add(
        document, 'show-selected-region-context-menu',
        (e: CustomEvent<SelectedRegionContextMenuData>) => {
          this.selectedRegionContextMenuX =
              e.detail.box.box.x - e.detail.box.box.width / 2;
          this.selectedRegionContextMenuY =
              e.detail.box.box.y + e.detail.box.box.height / 2;
          this.selectedRegionContextMenuBox = e.detail.box;
          this.detectedTextStartIndex = e.detail.selectionStartIndex;
          this.detectedTextEndIndex = e.detail.selectionEndIndex;
          this.showDetectedTextContextMenuOptions =
              this.detectedTextStartIndex !== -1 &&
              this.detectedTextEndIndex !== -1;
          this.showSelectedRegionContextMenu =
              (!this.suppressCopyAndSaveAsImage &&
               (this.enableCopyAsImage || this.enableSaveAsImage)) ||
              this.showDetectedTextContextMenuOptions;
        });
    this.eventTracker_.add(
        document, 'restore-selected-region-context-menu', () => {
          // show-selected-region-context-menu may not have been called yet if
          // we are still waiting for the text layer to receive text. Check for
          // this condition by checking if the box has been set.
          if (this.selectedRegionContextMenuBox !== undefined) {
            this.showSelectedRegionContextMenu =
                (!this.suppressCopyAndSaveAsImage &&
                 (this.enableCopyAsImage || this.enableSaveAsImage)) ||
                this.showDetectedTextContextMenuOptions;
          }
        });
    this.eventTracker_.add(
        document, 'hide-selected-region-context-menu', () => {
          this.showSelectedRegionContextMenu = false;
          this.detectedTextStartIndex = -1;
          this.detectedTextEndIndex = -1;
        });
    this.eventTracker_.add(document, 'darken-extra-scrim-opacity', () => {
      this.darkenExtraScrim = true;
    });
    this.eventTracker_.add(document, 'lighten-extra-scrim-opacity', () => {
      this.darkenExtraScrim = false;
    });
    this.eventTracker_.add(
        this.$.initialFlashScrim, 'animationend', (event: AnimationEvent) => {
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

    this.updateSelectionOverlayRect();
    this.updateDevicePixelRatioListener();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.resizeObserver.unobserve(this);
    this.eventTracker_.removeAll();
    this.listenerIds.forEach(
        id => assert(this.browserProxy.callbackRouter.removeListener(id)));
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

      this.$.cursor.style.transform =
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
  private setCursorToText() {
    // Set body cursor style to handle dragging.
    document.body.style.cursor = 'text';
    this.cursorOffsetX = 3;
    this.cursorOffsetY = 8;
    this.style.setProperty(CURSOR_IMG_URL, 'url("text.svg")');
  }

  // Called on region selection drag.
  private setCursorToCrosshair() {
    // Set body cursor style to handle dragging.
    document.body.style.cursor = 'crosshair';
    this.cursorOffsetX = 3;
    this.cursorOffsetY = 6;
    this.style.setProperty(CURSOR_IMG_URL, 'url("lens.svg")');
  }

  // Called on object hover.
  private setCursorToPointer() {
    // No dragging for objects, so no need to set body cursor style.
    this.cursorOffsetX = 11;
    this.cursorOffsetY = 17;
    this.style.setProperty(CURSOR_IMG_URL, 'url("lens.svg")');
  }

  private resetCursor() {
    if (this.translateModeEnabled) {
      // If translate mode is enabled, the default cursor state should be
      // text.
      this.setCursorToText();
      return;
    }
    document.body.style.cursor = 'unset';
    this.cursorOffsetX = 3;
    this.cursorOffsetY = 6;
    this.style.setProperty(CURSOR_IMG_URL, 'url("lens.svg")');
  }
  // LINT.ThenChange(//chrome/browser/resources/lens/overlay/cursor_tooltip.ts:CursorOffsetValues)

  private handlePointerEnter() {
    this.isPointerInside = true;
    if (!this.isPointerInsideContextMenu) {
      this.dispatchEvent(
          new CustomEvent<CursorTooltipData>('set-cursor-tooltip', {
            bubbles: true,
            composed: true,
            detail: {
              tooltipType: this.translateModeEnabled ?
                  CursorTooltipType.TEXT_HIGHLIGHT :
                  CursorTooltipType.REGION_SEARCH,
            },
          }));
    }
  }

  private handlePointerLeave() {
    this.isPointerInside = false;

    // Unfocus the shimmer from the cursor. If the cursor is dragging, force
    // shimmer to follow cursor.
    if (!this.disableShimmer &&
        this.currentGesture.state !== GestureState.DRAGGING) {
      unfocusShimmer(this, ShimmerControlRequester.CURSOR);
    }
  }

  private onImageRendered() {
    // Let the parent know it is safe to blur the background.
    this.dispatchEvent(new CustomEvent(
        'screenshot-rendered', {bubbles: true, composed: true}));
    this.browserProxy.handler.notifyOverlayInitialized();
  }

  private onPointerDown(event: PointerEvent) {
    if (this.shouldIgnoreEvent(event)) {
      return;
    }

    if (event.button === 2 /* right button */) {
      if (this.$.textSelectionLayer.handleRightClick(event)) {
        return;
      }
      this.$.postSelectionRenderer.handleRightClick(event);
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

    // If searchbox is stealing focus, we only want to respond to drag gestures,
    // so wait to send gesture started until a drag has happened.
    if (!this.isSearchboxFocused) {
      // If searchbox isn't stealing focus, start the gesture ASAP.
      this.handleGestureStart();
    }
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

  private handleGestureStart() {
    this.set('currentGesture.state', GestureState.STARTING);

    // Send events to hide UI.
    this.browserProxy.handler.closeSearchBubble();
    this.browserProxy.handler.closePreselectionBubble();
    this.suppressCopyAndSaveAsImage = false;
    this.dispatchEvent(
        new CustomEvent('selection-started', {bubbles: true, composed: true}));

    if (this.$.textSelectionLayer.handleGestureStart(this.currentGesture)) {
      // Text is responding to this sequence of gestures.
      this.draggingRespondent = DragFeature.TEXT;
      this.$.postSelectionRenderer.clearSelection();
    } else if (this.$.postSelectionRenderer.handleGestureStart(
                   this.currentGesture)) {
      this.draggingRespondent = DragFeature.POST_SELECTION;
    }
  }

  private handleGestureDrag(event: PointerEvent) {
    assert(this.currentGesture.state === GestureState.DRAGGING);
    // Capture pointer events so gestures still work if the users pointer
    // leaves the selection overlay div. Pointer capture is implicitly
    // released after pointerup or pointercancel events.
    this.setPointerCapture(event.pointerId);

    if (this.draggingRespondent === DragFeature.TEXT) {
      this.setCursorToText();
      this.$.textSelectionLayer.handleGestureDrag(this.currentGesture);
    } else if (this.draggingRespondent === DragFeature.POST_SELECTION) {
      this.$.postSelectionRenderer.handleGestureDrag(this.currentGesture);
    } else if (!this.translateModeEnabled) {
      // Let the features respond to the current drag if no other feature
      // responded first, but only if translate mode is not enabled.
      // The dragging responding may not be TEXT in translate mode if
      // there is no selectable text.
      this.setCursorToCrosshair();
      this.$.postSelectionRenderer.clearSelection();
      this.draggingRespondent = DragFeature.MANUAL_REGION;
      this.$.regionSelectionLayer.handleGestureDrag(this.currentGesture);
    }
  }

  private handleGestureEnd() {
    // Allow proper feature to respond to the tap/drag event.
    switch (this.currentGesture.state) {
      case GestureState.DRAGGING:

        // Drag has finished. Let the features respond to the end of a drag.
        if (this.draggingRespondent === DragFeature.MANUAL_REGION) {
          this.$.regionSelectionLayer.handleGestureEnd(this.currentGesture);
        } else if (this.draggingRespondent === DragFeature.TEXT) {
          this.$.textSelectionLayer.handleGestureEnd();
        } else if (this.draggingRespondent === DragFeature.POST_SELECTION) {
          this.$.postSelectionRenderer.handleGestureEnd();
        }
        break;
      case GestureState.STARTING:
        // This gesture was a tap. Let the features respond to a tap.
        if (this.draggingRespondent === DragFeature.TEXT) {
          this.$.textSelectionLayer.handleGestureEnd();
          break;
        }
        if (this.translateModeEnabled) {
          // Don't allow clicks to select objects or regions if translate
          // mode is enabled. The dragging respondent may not be TEXT if
          // there is no selectable text on the screen at all.
          break;
        }
        if (this.$.objectSelectionLayer.handleGestureEnd(this.currentGesture)) {
          break;
        }
        this.$.regionSelectionLayer.handleGestureEnd(this.currentGesture);
        break;
      default:  // Other states are invalid and ignored.
        break;
    }

    this.resetCursor();
    this.dispatchEvent(new CustomEvent('selection-finished', {
      bubbles: true,
      composed: true,
    }));
  }

  private handleGestureCancel() {
    this.$.textSelectionLayer.cancelGesture();
    this.$.regionSelectionLayer.cancelGesture();
    this.$.postSelectionRenderer.cancelGesture();
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

      // Set our own canvas size while preserving the canvas aspect ratio.
      const screenshotHeight = this.$.backgroundImageCanvas.height;
      const screenshotWidth = this.$.backgroundImageCanvas.width;

      const doesScreenshotFillContainer =
          Math.abs(
              newRect.width - (screenshotWidth / window.devicePixelRatio)) <=
              SCREENSHOT_RESIZE_TOLERANCE_PIXELS &&
          Math.abs(
              newRect.height - (screenshotHeight / window.devicePixelRatio)) <=
              SCREENSHOT_RESIZE_TOLERANCE_PIXELS;

      // Apply margins if the page is resized and not closing.
      const margins = !doesScreenshotFillContainer && !this.isClosing ?
          SCREENSHOT_FULLSIZE_MARGIN_PIXEL * 2 :
          0;
      const containerWidth = newRect.width - margins;
      const containerHeight = newRect.height - margins;

      // Get the aspect ratio to force the image to conform to.
      const aspectRatio = this.$.backgroundImageCanvas.width /
          this.$.backgroundImageCanvas.height;

      // Calculate potential dimensions based on width and height
      const widthBasedHeight = Math.round(containerWidth / aspectRatio);
      const heightBasedWidth = Math.round(containerHeight * aspectRatio);

      // Choose dimensions that fit within the container while preserving aspect
      // ratio
      if (widthBasedHeight <= containerHeight) {
        // Width-based dimensions fit
        this.canvasHeight = widthBasedHeight;
        this.canvasWidth = containerWidth;
      } else {
        // Height-based dimensions fit
        this.canvasWidth = heightBasedWidth;
        this.canvasHeight = containerHeight;
      }

      this.isResized = !doesScreenshotFillContainer;
      if (this.isResized) {
        this.isInitialSize = false;
        // The flash animation is cut short but animationend is never called if
        // the overlay is resized before animationend is called. This is because
        // the flash scrim is hidden on resize.
        this.onInitialFlashAnimationEnd();
      }

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
        this.$.selectionOverlay.offsetLeft, this.$.selectionOverlay.offsetTop,
        this.$.selectionOverlay.offsetWidth,
        this.$.selectionOverlay.offsetHeight);
  }

  private resizeSelectionCanvases(newWidth: number, newHeight: number) {
    this.$.regionSelectionLayer.setCanvasSizeTo(newWidth, newHeight);
    this.$.postSelectionRenderer.setCanvasSizeTo(newWidth, newHeight);
    this.$.objectSelectionLayer.setCanvasSizeTo(newWidth, newHeight);
    if (this.useShimmerCanvas) {
      this.$.overlayShimmerCanvas.setCanvasSizeTo(newWidth, newHeight);
    }
  }

  // Updates the currentGesture to correspond with the given PointerEvent.
  private updateGestureCoordinates(event: PointerEvent) {
    this.currentGesture.clientX = event.clientX;
    this.currentGesture.clientY = event.clientY;
  }

  // Returns if the given PointerEvent should be ignored.
  private shouldIgnoreEvent(event: PointerEvent) {
    if (this.isClosing) {
      return true;
    }
    const elementsAtPoint =
        this.shadowRoot!.elementsFromPoint(event.clientX, event.clientY);
    // Do not intercept events that should go to the following elements.
    if (elementsAtPoint.includes(this.$.selectedTextContextMenu) ||
        elementsAtPoint.includes(this.$.selectedRegionContextMenu)) {
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

  private getContextMenuStyle(contextMenuX: number, contextMenuY: number):
      string {
    return `left: ${toPercent(contextMenuX)}; top: calc(${
        toPercent(contextMenuY)} + 12px)`;
  }

  private async handleCopy() {
    if (this.textSelectionStartIndex < 0 || this.textSelectionEndIndex < 0) {
      return;
    }
    this.browserProxy.handler.copyText(this.highlightedText);
    recordLensOverlayInteraction(INVOCATION_SOURCE, UserAction.kCopyText);
    this.dispatchEvent(new CustomEvent('text-copied', {
      bubbles: true,
      composed: true,
    }));
    this.showSelectedTextContextMenu = false;
  }

  private handleSelectText() {
    this.$.textSelectionLayer.selectAndSendWords(
        this.detectedTextStartIndex, this.detectedTextEndIndex);
    this.$.postSelectionRenderer.clearSelection();
    unfocusShimmer(this, ShimmerControlRequester.CURSOR);
  }

  private handleTranslateDetectedText() {
    this.$.textSelectionLayer.selectAndTranslateWords(
        this.detectedTextStartIndex, this.detectedTextEndIndex);
    this.$.postSelectionRenderer.clearSelection();
    unfocusShimmer(this, ShimmerControlRequester.CURSOR);
  }

  private handleTranslate() {
    BrowserProxyImpl.getInstance().handler.issueTranslateSelectionRequest(
        this.highlightedText, this.contentLanguage,
        this.textSelectionStartIndex, this.textSelectionEndIndex);
    this.showSelectedTextContextMenu = false;
    recordLensOverlayInteraction(INVOCATION_SOURCE, UserAction.kTranslateText);
  }

  private handleCopyAsImage() {
    BrowserProxyImpl.getInstance().handler.copyImage(
        this.selectedRegionContextMenuBox);
    this.showSelectedRegionContextMenu = false;
    recordLensOverlayInteraction(INVOCATION_SOURCE, UserAction.kCopyAsImage);
    this.dispatchEvent(new CustomEvent('copied-as-image', {
      bubbles: true,
      composed: true,
    }));
  }

  private handleSaveAsImage() {
    BrowserProxyImpl.getInstance().handler.saveAsImage(
        this.selectedRegionContextMenuBox);
    this.showSelectedRegionContextMenu = false;
    recordLensOverlayInteraction(INVOCATION_SOURCE, UserAction.kSaveAsImage);
  }

  // Make the cursor disappear over the context menu, as if leaving the overlay.
  private handlePointerEnterContextMenu() {
    this.isPointerInside = false;
    this.isPointerInsideContextMenu = true;
    // Hide the cursor tooltip.
    this.dispatchEvent(
        new CustomEvent<CursorTooltipData>('set-cursor-tooltip', {
          bubbles: true,
          composed: true,
          detail: {tooltipType: CursorTooltipType.NONE},
        }));
    unfocusShimmer(this, ShimmerControlRequester.CURSOR);
  }

  private handlePointerLeaveContextMenu() {
    this.isPointerInside = true;
    this.isPointerInsideContextMenu = false;
    // Reshow the cursor tooltip.
    this.dispatchEvent(
        new CustomEvent<CursorTooltipData>('set-cursor-tooltip', {
          bubbles: true,
          composed: true,
          detail: {
            tooltipType: this.translateModeEnabled ?
                CursorTooltipType.TEXT_HIGHLIGHT :
                CursorTooltipType.REGION_SEARCH,
          },
        }));
  }

  private onInitialFlashAnimationEnd() {
    if (this.hasInitialFlashAnimationEnded) {
      return;
    }
    this.hasInitialFlashAnimationEnded = true;
    this.eventTracker_.remove(this.$.initialFlashScrim, 'animationend');

    this.browserProxy.handler.addBackgroundBlur();

    // Let the parent know the initial flash image animation has finished.
    this.dispatchEvent(new CustomEvent(
        'initial-flash-animation-end', {bubbles: true, composed: true}));

    // Don't start the shimmer animation until the initial flash animation is
    // finished.
    if (!this.disableShimmer) {
      if (this.useShimmerCanvas) {
        this.$.overlayShimmerCanvas.startAnimation();
      } else {
        this.$.overlayShimmer.startAnimation();
      }
    }
  }

  private async screenshotDataReceived(screenshotBitmap: ImageBitmap) {
    renderScreenshot(this.$.backgroundImageCanvas, screenshotBitmap);
    // Start the canvas as the same dimensions as the viewport, since we are
    // assuming the screenshot takes up the viewport dimensions. Our resize
    // handler will adjust as needed.
    this.canvasWidth = window.innerWidth;
    this.canvasHeight = window.innerHeight;

    this.isScreenshotRendered = true;
    this.onImageRendered();
  }

  fetchNewScreenshotForTesting() {
    ScreenshotBitmapBrowserProxyImpl.getInstance().fetchScreenshot(
        this.screenshotDataReceived.bind(this));
  }

  getShowSelectedRegionContextMenuForTesting() {
    return this.showSelectedRegionContextMenu;
  }

  getShowSelectedTextContextMenuForTesting() {
    return this.showSelectedTextContextMenu;
  }

  getShowDetectedTextContextMenuOptionsForTesting() {
    return this.showDetectedTextContextMenuOptions;
  }

  getSuppressCopyAndSaveAsImageForTesting() {
    return this.suppressCopyAndSaveAsImage;
  }

  handleSelectTextForTesting() {
    this.handleSelectText();
  }

  handleTranslateDetectedTextForTesting() {
    this.handleTranslateDetectedText();
  }

  handleCopyForTesting() {
    this.handleCopy();
  }

  handleTranslateForTesting() {
    this.handleTranslate();
  }

  handleCopyAsImageForTesting() {
    this.handleCopyAsImage();
  }

  handleSaveAsImageForTesting() {
    this.handleSaveAsImage();
  }

  setSearchboxFocusForTesting(isFocused: boolean) {
    this.isSearchboxFocused = isFocused;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-selection-overlay': SelectionOverlayElement;
  }
}

customElements.define(SelectionOverlayElement.is, SelectionOverlayElement);
