// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './object_layer.js';
import './simplified_text_layer.js';
import './text_layer.js';
import './region_selection.js';
import './post_selection_renderer.js';
import './overlay_border_glow.js';
import './overlay_shimmer_canvas.js';
import '/strings.m.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getFallbackTheme} from './color_utils.js';
import {type CursorTooltipData, CursorTooltipType} from './cursor_tooltip.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import {UserAction} from './lens.mojom-webui.js';
import type {OverlayTheme} from './lens.mojom-webui.js';
import {INVOCATION_SOURCE} from './lens_overlay_app.js';
import {ContextMenuOption, recordContextMenuOptionShown, recordLensOverlayInteraction, recordLensOverlaySelectionCloseButtonShown, recordLensOverlaySelectionCloseButtonUsed} from './metrics_utils.js';
import type {ObjectLayerElement} from './object_layer.js';
import type {OverlayBorderGlowElement} from './overlay_border_glow.js';
import type {OverlayShimmerCanvasElement} from './overlay_shimmer_canvas.js';
import type {PostSelectionBoundingBox, PostSelectionRendererElement} from './post_selection_renderer.js';
import type {RegionSelectionElement} from './region_selection.js';
import {ScreenshotBitmapBrowserProxyImpl} from './screenshot_bitmap_browser_proxy.js';
import {renderScreenshot} from './screenshot_utils.js';
import {getTemplate} from './selection_overlay.html.js';
import {CursorType, DRAG_THRESHOLD, DragFeature, emptyGestureEvent, focusShimmerOnRegion, GestureState, ShimmerControlRequester} from './selection_utils.js';
import type {GestureEvent, OverlayShimmerFocusedRegion} from './selection_utils.js';
import type {SimplifiedTextLayerElement} from './simplified_text_layer.js';
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

// Returns true if the event is a keystroke that should not activate a control.
function shouldIgnoreKeyboardEvent(event: Event|undefined): boolean {
  return event instanceof KeyboardEvent &&
      !(event.key === 'Enter' || event.key === ' ');
}

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
  // The text selection that the context menu commands will act on.
  text?: string;
}

export interface SelectionOverlayElement {
  $: {
    backgroundImageCanvas: HTMLCanvasElement,
    closeButton: CrIconButtonElement,
    cursor: HTMLElement,
    initialFlashScrim: HTMLElement,
    objectSelectionLayer: ObjectLayerElement,
    overlayShimmerCanvas: OverlayShimmerCanvasElement,
    postSelectionRenderer: PostSelectionRendererElement,
    regionSelectionLayer: RegionSelectionElement,
    selectedRegionContextMenu: HTMLElement,
    selectedTextContextMenu: HTMLElement,
    selectionOverlay: HTMLElement,
    selectTextContextMenuItem: HTMLElement,
    textLayer: SimplifiedTextLayerElement,
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
      showTranslateContextMenuItem: {
        type: Boolean,
        reflectToAttribute: true,
        value: true,
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
      selectedRegionContextMenuHorizontalStyle: String,
      selectedRegionContextMenuVerticalStyle: String,
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
      enableCopyAsImage: {
        type: Boolean,
        reflectToAttribute: true,
        value: () => loadTimeData.getBoolean('enableCopyAsImage'),
      },
      enableSaveAsImage: {
        type: Boolean,
        reflectToAttribute: true,
        value: () => loadTimeData.getBoolean('enableSaveAsImage'),
      },
      isClosing: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      suppressCopyAndSaveAsImage: {
        type: Boolean,
        reflectToAttribute: true,
        value: () => {
          return loadTimeData.getString('invocationSource') ===
              'ContentAreaContextMenuImage';
        },
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
      translateModeEnabled: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      selectionOverlayRect: Object,
      isSearchboxFocused: Boolean,
      areLanguagePickersOpen: Boolean,
      sidePanelOpened: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      hideBackgroundImageCanvas: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      enableRegionContextMenu: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
      },
    };
  }

  // Whether the screenshot has finished loading in.
  declare private isScreenshotRendered: boolean;
  // Whether the selection overlay is its initial size, or has changed size.
  declare private isResized: boolean;
  declare private isInitialSize: boolean;
  declare private showTranslateContextMenuItem: boolean;
  declare private showSelectedTextContextMenu: boolean;
  declare private showSelectedRegionContextMenu: boolean;
  declare private showDetectedTextContextMenuOptions: boolean;
  // Location at which to show the context menus.
  declare private selectedTextContextMenuX: number;
  declare private selectedTextContextMenuY: number;
  declare private selectedRegionContextMenuX: number;
  declare private selectedRegionContextMenuY: number;
  // Width and height values for rendering the background image canvas as the
  // proper dimensions.
  declare private canvasHeight: number;
  declare private canvasWidth: number;
  declare private selectedRegionContextMenuHorizontalStyle: string;
  declare private selectedRegionContextMenuVerticalStyle: string;
  // The current content rectangle of the selection elements DIV. This is the
  // bounds of the screenshot and the part the user interacts with. This should
  // be used instead of call getBoundingClientRect().
  declare private selectionOverlayRect: DOMRect;
  // Whether the users focus is currently in the overlay searchbox. Passed in
  // from parent.
  declare private isSearchboxFocused: boolean;
  // Whether any of the language pickers are currently open. Passed in from
  // parent.
  declare private areLanguagePickersOpen: boolean;

  // The selected region on which the context menu is being displayed. Used as
  // argument for copy and save as image calls.
  private selectedRegionContextMenuBox: CenterRotatedBox;
  private highlightedText: string = '';
  private contentLanguage: string = '';
  private textSelectionStartIndex: number = -1;
  private textSelectionEndIndex: number = -1;
  private detectedTextStartIndex: number = -1;
  private detectedTextEndIndex: number = -1;
  declare private isPointerInside;
  private isPointerInsideButton = false;
  // The current gesture event. The coordinate values are only accurate if a
  // gesture has started.
  declare private currentGesture: GestureEvent;
  declare private disableShimmer: boolean;
  declare private enableBorderGlow: boolean;
  declare private enableCopyAsImage: boolean;
  declare private enableSaveAsImage: boolean;
  declare private suppressCopyAndSaveAsImage: boolean;
  // Whether the overlay is being shut down.
  declare private isClosing: boolean;
  // Whether the default background scrim is currently being darkened.
  declare private darkenExtraScrim: boolean;
  // Whether the shimmer is currently focused on a segmentation mask.
  declare private shimmerOnSegmentation: boolean;
  declare private shimmerFadeOutComplete: boolean;
  // Whether the side panel is currently opened.
  declare private sidePanelOpened: boolean;
  // Whether the background image canvas should currently be shown.
  declare private hideBackgroundImageCanvas: boolean;
  // Whether the region context menu is enabled.
  declare private enableRegionContextMenu: boolean;

  // The border glow layer rendered on the selection overlay if it exists.
  private overlayBorderGlow: OverlayBorderGlowElement;

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

  // Whether the close button used metric was recorded in this session.
  private closeButtonUsedRecorded = false;

  declare private theme: OverlayTheme;

  // Whether or not translate mode is enabled. If true, only text should
  // be selectable, and it should be selectable from any point in the
  // overlay.
  declare private translateModeEnabled: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.resizeObserver.observe(this);
    this.listenerIds = [
      this.browserProxy.callbackRouter.notifyOverlayClosing.addListener(() => {
        this.isClosing = true;
        this.removeDragListeners();
      }),
      this.browserProxy.callbackRouter.onCopyCommand.addListener(
          this.onCopyCommand.bind(this)),
      this.browserProxy.callbackRouter.notifyResultsPanelOpened.addListener(
          this.onNotifyResultsPanelOpened.bind(this)),
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
          this.selectedTextContextMenuX = e.detail.left;
          this.selectedTextContextMenuY = e.detail.bottom;
          this.highlightedText = e.detail.text;
          this.contentLanguage = e.detail.contentLanguage;
          this.textSelectionStartIndex = e.detail.selectionStartIndex;
          this.textSelectionEndIndex = e.detail.selectionEndIndex;
          this.setShowSelectedTextContextMenu(true);
        });
    this.eventTracker_.add(
        document, 'restore-selected-text-context-menu', () => {
          // show-selected-text-context-menu or
          // update-selected-text-context-menu must be triggered first so that
          // instance variables are set.
          this.setShowSelectedTextContextMenu(true);
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
      this.setShowSelectedTextContextMenu(false);
      this.textSelectionStartIndex = -1;
      this.textSelectionEndIndex = -1;
    });
    this.eventTracker_.add(
        document, 'update-selected-region-context-menu',
        (e: CustomEvent<SelectedRegionContextMenuData>) => {
          this.updateSelectedRegionContextMenu(e.detail);
          this.positionSelectedRegionContextMenu();
        });
    this.eventTracker_.add(
        document, 'show-selected-region-context-menu',
        (e: CustomEvent<SelectedRegionContextMenuData>) => {
          this.updateSelectedRegionContextMenu(e.detail);
          this.setShowSelectedRegionContextMenu(
              (!this.suppressCopyAndSaveAsImage &&
               (this.enableCopyAsImage || this.enableSaveAsImage)) ||
              this.showDetectedTextContextMenuOptions);
          this.positionSelectedRegionContextMenu();

          // Send an event to the post selection renderer to darken the scrim if
          // text is found within the region so that text gleams are visible.
          if (this.showDetectedTextContextMenuOptions) {
            this.dispatchEvent(new CustomEvent('text-found-in-region', {
              bubbles: true,
              composed: true,
            }));
          }
        });
    this.eventTracker_.add(
        document, 'restore-selected-region-context-menu', () => {
          // show-selected-region-context-menu may not have been called yet if
          // we are still waiting for the text layer to receive text. Check for
          // this condition by checking if the box has been set.
          if (this.selectedRegionContextMenuBox !== undefined) {
            this.setShowSelectedRegionContextMenu(
                (!this.suppressCopyAndSaveAsImage &&
                 (this.enableCopyAsImage || this.enableSaveAsImage)) ||
                this.showDetectedTextContextMenuOptions);
          }
        });
    this.eventTracker_.add(
        document, 'hide-selected-region-context-menu', () => {
          this.setShowSelectedRegionContextMenu(false);
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
      this.eventTracker_.add(
          document, 'post-selection-updated', (e: CustomEvent) => {
            this.selectedRegionContextMenuBox = e.detail.centerRotatedBox;
            this.selectedRegionContextMenuX =
                this.selectedRegionContextMenuBox.box.x -
                this.selectedRegionContextMenuBox.box.width / 2;
            this.selectedRegionContextMenuY =
                this.selectedRegionContextMenuBox.box.y +
                this.selectedRegionContextMenuBox.box.height / 2;
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

  protected onIsResizedChanged(newValue: boolean): void {
    this.browserProxy.handler.setLiveBlur(newValue);
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
    if (!this.isPointerInsideButton) {
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
  }

  private onImageRendered() {
    // Let the parent know it is safe to blur the background.
    this.dispatchEvent(new CustomEvent('screenshot-rendered', {
      bubbles: true,
      composed: true,
      detail: {isSidePanelOpen: this.sidePanelOpened},
    }));
    this.browserProxy.handler.notifyOverlayInitialized();
  }

  private onPointerDown(event: PointerEvent) {
    if (this.shouldIgnoreEvent(event)) {
      return;
    }

    if (event.button === 2 /* right button */) {
      if (this.$.textLayer.handleRightClick(event)) {
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

    // Try to close the translate feature promo if it is currently active. No-op
    // if it is not active.
    this.browserProxy.handler.maybeCloseTranslateFeaturePromo(
        /*featureEngaged=*/ false);

    // If searchbox is stealing focus, we only want to respond to drag gestures,
    // so wait to send gesture started until a drag has happened. This is also
    // the case if the language pickers are currently open.
    if (!this.isSearchboxFocused && !this.areLanguagePickersOpen) {
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
    this.browserProxy.handler.closePreselectionBubble();
    this.suppressCopyAndSaveAsImage = false;
    this.dispatchEvent(
        new CustomEvent('selection-started', {bubbles: true, composed: true}));

    // The context menu should have text reset whenever a new selection is
    // started.
    this.detectedTextStartIndex = -1;
    this.detectedTextEndIndex = -1;
    this.showDetectedTextContextMenuOptions = false;

    this.$.textLayer.onSelectionStart();
    if (this.enableBorderGlow) {
      this.getOverlayBorderGlow().handleGestureStart();

      // If there is no post selection, fade the scrim from the region selection
      // back in.
      if (!this.$.postSelectionRenderer.hasSelection()) {
        // TODO(crbug.com/421002691): follow the convention where the layer
        // should return true if its handling the gesture, and
        // draggingRespondent should be updated. Currently used to trigger the
        // fade in of the darkened scrim.
        this.$.regionSelectionLayer.handleGestureStart();
      }
    }

    if (this.$.postSelectionRenderer.handleGestureStart(this.currentGesture)) {
      this.draggingRespondent = DragFeature.POST_SELECTION;
    } else if (this.$.textLayer.handleGestureStart(this.currentGesture)) {
      // Text is responding to this sequence of gestures.
      this.draggingRespondent = DragFeature.TEXT;
      this.$.postSelectionRenderer.clearSelection();
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
      this.$.textLayer.handleGestureDrag(this.currentGesture);
    } else if (this.draggingRespondent === DragFeature.POST_SELECTION) {
      this.$.postSelectionRenderer.handleGestureDrag(this.currentGesture);
    } else if (!this.translateModeEnabled) {
      // Let the features respond to the current drag if no other feature
      // responded first, but only if translate mode is not enabled.
      // The dragging responding may not be TEXT in translate mode if
      // there is no selectable text.
      this.setCursorToCrosshair();
      this.draggingRespondent = DragFeature.MANUAL_REGION;
      this.$.postSelectionRenderer.clearSelection();

      // TODO(crbug.com/421002691): follow the convention where the layer
      // should return true if its handling the gesture, and draggingRespondent
      // should be updated. Currently used to trigger the fade in of the
      // darkened scrim.
      this.$.regionSelectionLayer.handleGestureStart();
      this.$.regionSelectionLayer.handleGestureDrag(this.currentGesture);
    }
  }

  private handleGestureEnd() {
    // Call onSelectionFinish before gesture is handled so the simplified text
    // layer can reset the context menu.
    this.$.textLayer.onSelectionFinish();

    // Allow proper feature to respond to the tap/drag event.
    switch (this.currentGesture.state) {
      case GestureState.DRAGGING:

        // Drag has finished. Let the features respond to the end of a drag.
        if (this.draggingRespondent === DragFeature.MANUAL_REGION) {
          this.$.regionSelectionLayer.handleGestureEnd(this.currentGesture);
        } else if (this.draggingRespondent === DragFeature.TEXT) {
          this.$.textLayer.handleGestureEnd();
        } else if (this.draggingRespondent === DragFeature.POST_SELECTION) {
          this.$.postSelectionRenderer.handleGestureEnd();
          // Fade out scrim which is currently being managed by region selection
          // TODO(crbug.com/420998632): move scrim out to its own component
          this.$.regionSelectionLayer.handlePostSelectionDragGestureEnd();
        }
        break;
      case GestureState.STARTING:
        // This gesture was a tap. Let the features respond to a tap.
        if (this.draggingRespondent === DragFeature.TEXT) {
          this.$.textLayer.handleGestureEnd();
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
    this.$.textLayer.cancelGesture();
    this.$.regionSelectionLayer.cancelGesture();
    this.$.postSelectionRenderer.cancelGesture();
  }

  private handlePostSelectionUpdated(height: number, width: number) {
    const overlayBorderGlow = this.getOverlayBorderGlow();
    // If there is no selection happening, fade the glow back in.
    if (width === 0 && height === 0 &&
        this.draggingRespondent === DragFeature.NONE) {
      overlayBorderGlow.handleClearSelection();
      this.$.regionSelectionLayer.handlePostSelectionCleared();
      return;
    }

    overlayBorderGlow.handlePostSelectionUpdated();
  }

  private updateCanvasSize(containerWidth: number, containerHeight: number) {
    // Set our own canvas size while preserving the canvas aspect ratio.
    const screenshotHeight = this.$.backgroundImageCanvas.height;
    const screenshotWidth = this.$.backgroundImageCanvas.width;

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
    const aspectRatio = this.$.backgroundImageCanvas.width /
        this.$.backgroundImageCanvas.height;

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
      this.positionSelectedRegionContextMenu();

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
    this.$.overlayShimmerCanvas.setCanvasSizeTo(newWidth, newHeight);
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
        elementsAtPoint.includes(this.$.selectedRegionContextMenu) ||
        elementsAtPoint.includes(this.$.closeButton)) {
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

  // Repositions the context menu to keep it inside the viewport.
  private positionSelectedRegionContextMenu() {
    if (!this.selectedRegionContextMenuBox) {
      return;
    }

    const left = this.selectedRegionContextMenuBox.box.x -
        this.selectedRegionContextMenuBox.box.width / 2;
    const top = this.selectedRegionContextMenuBox.box.y -
        this.selectedRegionContextMenuBox.box.height / 2;
    const bottom = this.selectedRegionContextMenuBox.box.y +
        this.selectedRegionContextMenuBox.box.height / 2;

    // First try to left-align to region.
    this.selectedRegionContextMenuHorizontalStyle =
        `left: ${toPercent(left)}; `;
    if (this.$.selectedRegionContextMenu.offsetLeft +
            this.$.selectedRegionContextMenu.offsetWidth >
        this.canvasWidth) {
      // If menu overflows right, right-align to region.
      this.selectedRegionContextMenuHorizontalStyle = `right: 0; `;
      if (this.$.selectedRegionContextMenu.offsetLeft < 0) {
        // If menu overflows left, allow constraining on both sides.
        this.selectedRegionContextMenuHorizontalStyle = ` `;
      }
    }

    // First try to position below region.
    this.selectedRegionContextMenuVerticalStyle =
        `top: calc(${toPercent(bottom)} + 12px)`;
    if (this.$.selectionOverlay.offsetTop +
            this.$.selectedRegionContextMenu.offsetTop +
            this.$.selectedRegionContextMenu.offsetHeight >
        window.innerHeight) {
      // If menu overflows bottom, position above region.
      this.selectedRegionContextMenuVerticalStyle =
          `bottom: calc(${toPercent(1 - top)} + 12px);`;
      if (this.$.selectionOverlay.offsetTop +
              this.$.selectedRegionContextMenu.offsetTop <
          0) {
        // If menu overflows top, position at top of viewport, overlapping the
        // region.
        this.selectedRegionContextMenuVerticalStyle =
            `bottom: calc(${toPercent(1 - top)} + 12px + ${
                this.$.selectionOverlay.offsetTop +
                this.$.selectedRegionContextMenu.offsetTop}px);`;
      }
    }
  }

  private getContextMenuStyle(contextMenuX: number, contextMenuY: number):
      string {
    return `left: ${toPercent(contextMenuX)}; top: calc(${
        toPercent(contextMenuY)} + 12px)`;
  }

  // This handles the copying of currently selected text on the overlay. This
  // differs from handleCopyDetectedText() since text must be selected in order
  // to copy.
  private handleCopy(event?: Event) {
    if (shouldIgnoreKeyboardEvent(event)) {
      return;
    }
    this.copyText(
        this.textSelectionStartIndex, this.textSelectionEndIndex,
        this.highlightedText);
  }

  // This handles the copying of detected text on the overlay within a selected
  // region. This differs from handleCopy() since text does not need to be
  // selected to support this copy.
  private handleCopyDetectedText(event?: Event) {
    if (shouldIgnoreKeyboardEvent(event)) {
      return;
    }
    this.setShowSelectedRegionContextMenu(false);

    this.$.textLayer.onCopyDetectedText(
        this.detectedTextStartIndex, this.detectedTextEndIndex,
        this.copyText.bind(this));
  }

  private copyText(textStartIndex: number, textEndIndex: number, text: string) {
    if (textStartIndex < 0 || textEndIndex < 0) {
      return;
    }
    this.browserProxy.handler.copyText(text);
    recordLensOverlayInteraction(INVOCATION_SOURCE, UserAction.kCopyText);
    this.dispatchEvent(new CustomEvent('text-copied', {
      bubbles: true,
      composed: true,
    }));
    this.setShowSelectedTextContextMenu(false);
    this.setShowSelectedRegionContextMenu(false);
  }

  private handleSelectText(event?: Event) {
    if (shouldIgnoreKeyboardEvent(event)) {
      return;
    }
    this.$.textLayer.selectAndSendWords(
        this.detectedTextStartIndex, this.detectedTextEndIndex);
    this.$.postSelectionRenderer.clearSelection();
  }

  private handleTranslateDetectedText(event?: Event) {
    if (shouldIgnoreKeyboardEvent(event)) {
      return;
    }
    this.$.textLayer.selectAndTranslateWords(
        this.detectedTextStartIndex, this.detectedTextEndIndex);

    // Do not clear the post selection renderer. Instead, just hide the region
    // context menu manually.
    this.setShowSelectedRegionContextMenu(false);
  }

  private handleTranslate(event?: Event) {
    if (shouldIgnoreKeyboardEvent(event)) {
      return;
    }
    BrowserProxyImpl.getInstance().handler.issueTranslateSelectionRequest(
        this.highlightedText.replaceAll('\r\n', ' '), this.contentLanguage,
        this.textSelectionStartIndex, this.textSelectionEndIndex);
    this.setShowSelectedTextContextMenu(false);
    recordLensOverlayInteraction(INVOCATION_SOURCE, UserAction.kTranslateText);
  }

  private handleCopyAsImage(event?: Event) {
    if (shouldIgnoreKeyboardEvent(event)) {
      return;
    }
    BrowserProxyImpl.getInstance().handler.copyImage(
        this.selectedRegionContextMenuBox);
    this.setShowSelectedRegionContextMenu(false);
    recordLensOverlayInteraction(INVOCATION_SOURCE, UserAction.kCopyAsImage);
    this.dispatchEvent(new CustomEvent('copied-as-image', {
      bubbles: true,
      composed: true,
    }));
  }

  private handleSaveAsImage(event?: Event) {
    if (shouldIgnoreKeyboardEvent(event)) {
      return;
    }
    BrowserProxyImpl.getInstance().handler.saveAsImage(
        this.selectedRegionContextMenuBox);
    this.setShowSelectedRegionContextMenu(false);
    recordLensOverlayInteraction(INVOCATION_SOURCE, UserAction.kSaveAsImage);
  }

  // Make the cursor disappear when entering selectable buttons, as if leaving the overlay.
  private handlePointerEnterButton() {
    this.isPointerInside = false;
    this.isPointerInsideButton = true;
    // Hide the cursor tooltip.
    this.dispatchEvent(
        new CustomEvent<CursorTooltipData>('set-cursor-tooltip', {
          bubbles: true,
          composed: true,
          detail: {tooltipType: CursorTooltipType.NONE},
        }));
  }

  private handlePointerLeaveButton() {
    this.isPointerInside = true;
    this.isPointerInsideButton = false;
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

  // Sets the text context menu to be visible or not, and logs the shown
  // context menu options.
  private setShowSelectedTextContextMenu(shouldShow: boolean) {
    if (shouldShow && !this.showSelectedTextContextMenu) {
      // If the context menu was not being shown earlier, but will be now, log
      // the shown context menu options.
      recordContextMenuOptionShown(
          INVOCATION_SOURCE, ContextMenuOption.COPY_TEXT);
      if (this.showTranslateContextMenuItem) {
        recordContextMenuOptionShown(
            INVOCATION_SOURCE, ContextMenuOption.TRANSLATE_TEXT);
      }
    }
    this.showSelectedTextContextMenu = shouldShow;
  }

  // Sets the region context menu to be visible or not, and logs the shown
  // context menu options.
  private setShowSelectedRegionContextMenu(shouldShow: boolean) {
    if (shouldShow && !this.showSelectedRegionContextMenu) {
      // If the context menu was not being shown earlier, but will be now, log
      // the shown context menu options.
      if (this.showDetectedTextContextMenuOptions) {
        // A copy text in region context menu option is shown.
        recordContextMenuOptionShown(
            INVOCATION_SOURCE, ContextMenuOption.COPY_TEXT_IN_REGION);
        if (this.showTranslateContextMenuItem) {
          recordContextMenuOptionShown(
              INVOCATION_SOURCE, ContextMenuOption.TRANSLATE_TEXT_IN_REGION);
        }
      }
      if (!this.suppressCopyAndSaveAsImage) {
        if (this.enableCopyAsImage) {
          recordContextMenuOptionShown(
              INVOCATION_SOURCE, ContextMenuOption.COPY_AS_IMAGE);
        }
        if (this.enableSaveAsImage) {
          recordContextMenuOptionShown(
              INVOCATION_SOURCE, ContextMenuOption.SAVE_AS_IMAGE);
        }
      }
    }
    this.showSelectedRegionContextMenu = shouldShow;
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
    if (!this.disableShimmer && !this.enableBorderGlow) {
      this.$.overlayShimmerCanvas.startAnimation();
    }
  }

  private screenshotDataReceived(
      screenshotBitmap: ImageBitmap, isSidePanelOpen: boolean) {
    renderScreenshot(this.$.backgroundImageCanvas, screenshotBitmap);
    // Start the canvas as the same dimensions as the viewport, since we are
    // assuming the screenshot takes up the viewport dimensions. Our resize
    // handler will adjust as needed.
    this.canvasWidth = window.innerWidth;
    this.canvasHeight = window.innerHeight;

    // This is the first time the screenshot has been rendered.
    this.isScreenshotRendered = true;
    if (isSidePanelOpen) {
      // Reset the state of the selection overlay to represent the overlay being
      // opened with the side panel open.
      this.sidePanelOpened = true;
      this.isResized = true;
      this.isInitialSize = false;

      // In the case of an overlay being shown with an already open side panel,
      // the region context menu should not be shown. Disable text highlights
      // as the text is not actionable anymore.
      this.enableRegionContextMenu = false;
      this.$.textLayer.disableHighlights();
    }
    this.onImageRendered();
  }

  private onOverlayReshown(screenshotBitmap: ImageBitmap) {
    // Render the new screenshot.
    renderScreenshot(this.$.backgroundImageCanvas, screenshotBitmap);

    // Reset the state of the selection overlay to represent the overlay being
    // opened with the side panel open.
    this.isClosing = false;
    this.sidePanelOpened = true;
    this.hideBackgroundImageCanvas = true;
    this.enableRegionContextMenu = false;

    this.updateCanvasSize(window.innerWidth, window.innerHeight);

    // Update our cached selection overlay rect to the new bounds.
    this.updateSelectionOverlayRect();
    this.resizeSelectionCanvases(
        this.selectionOverlayRect.width, this.selectionOverlayRect.height);

    // Allow the new screenshot to render / allow any resizing that needs to
    // happen before finishing the reshow overlay flow. This needs an extra
    // animation frame after the next render to ensure the new screenshot is
    // painted at least once.
    afterNextRender(this.$.backgroundImageCanvas, () => {
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

  private onCopyCommand() {
    this.$.textLayer.onCopyDetectedText(
        this.detectedTextStartIndex, this.detectedTextEndIndex,
        this.copyText.bind(this));
  }

  private updateSelectedRegionContextMenu(data: SelectedRegionContextMenuData) {
    this.selectedRegionContextMenuX = data.box.box.x - data.box.box.width / 2;
    this.selectedRegionContextMenuY = data.box.box.y + data.box.box.height / 2;
    this.selectedRegionContextMenuBox = data.box;
    this.detectedTextStartIndex = data.selectionStartIndex;
    this.detectedTextEndIndex = data.selectionEndIndex;
    this.showDetectedTextContextMenuOptions =
        this.detectedTextStartIndex !== -1 && this.detectedTextEndIndex !== -1;
    this.highlightedText = data.text ?? this.highlightedText;
  }

  private onNotifyResultsPanelOpened() {
    // No-op if the side panel was already opened.
    if (this.sidePanelOpened) {
      return;
    }
    // The close button should be showing on the selection overlay. Record this
    // as a close button impression if the side panel was not already opened.
    recordLensOverlaySelectionCloseButtonShown(INVOCATION_SOURCE);
    this.sidePanelOpened = true;
  }

  private onCloseButtonClick() {
    // If the user manages to click the close button multiple times, only
    // record the first click. This is to avoid overcounting the number of
    // times the close button is used.
    if (!this.closeButtonUsedRecorded) {
      recordLensOverlaySelectionCloseButtonUsed(INVOCATION_SOURCE);
      this.closeButtonUsedRecorded = true;
    }
    this.browserProxy.handler.closeRequestedByOverlayCloseButton();
  }

  private onFinishReshowOverlay() {
    this.hideBackgroundImageCanvas = false;
    recordLensOverlaySelectionCloseButtonShown(INVOCATION_SOURCE);
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

  handleCopyDetectedTextForTesting() {
    this.handleCopyDetectedText();
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

  setLanguagePickersOpenForTesting(open: boolean) {
    this.areLanguagePickersOpen = open;
  }

  getHideBackgroundImageCanvasForTesting() {
    return this.hideBackgroundImageCanvas;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-selection-overlay': SelectionOverlayElement;
  }
}

customElements.define(SelectionOverlayElement.is, SelectionOverlayElement);
