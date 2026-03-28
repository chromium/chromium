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
import '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {type CursorTooltipData, CursorTooltipType} from './cursor_tooltip.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import {UserAction} from './lens.mojom-webui.js';
import {INVOCATION_SOURCE} from './lens_overlay_app.js';
import {ContextMenuOption, recordContextMenuOptionShown, recordLensOverlayInteraction, recordLensOverlaySelectionCloseButtonShown, recordLensOverlaySelectionCloseButtonUsed} from './metrics_utils.js';
import type {ObjectLayerElement} from './object_layer.js';
import type {OverlayShimmerCanvasElement} from './overlay_shimmer_canvas.js';
import type {PostSelectionRendererElement} from './post_selection_renderer.js';
import type {RegionSelectionElement} from './region_selection.js';
import {getTemplate} from './selection_overlay.html.js';
import {CURSOR_IMG_URL, SelectionOverlayBaseElement} from './selection_overlay_base.js';
import {DragFeature, GestureState} from './selection_utils.js';
import type {SimplifiedTextLayerElement} from './simplified_text_layer.js';
import type {TranslateState} from './translate_button.js';
import {toPercent} from './values_converter.js';

// Returns true if the event is a keystroke that should not activate a control.
function shouldIgnoreKeyboardEvent(event: Event|undefined): boolean {
  return event instanceof KeyboardEvent &&
      !(event.key === 'Enter' || event.key === ' ');
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

/*
 * Element responsible for coordinating selections between the various selection
 * features. This includes:
 *   - Storing state needed to coordinate selections across features
 *   - Listening to mouse/tap events and delegating them to the correct features
 *   - Coordinating animations between the different features
 */
export class SelectionOverlayElement extends SelectionOverlayBaseElement {
  static get is() {
    return 'lens-selection-overlay';
  }

  static get template() {
    return getTemplate();
  }

  static override get properties() {
    return {
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
      suppressCopyAndSaveAsImage: {
        type: Boolean,
        reflectToAttribute: true,
        value: () => {
          return loadTimeData.getString('invocationSource') ===
              'ContentAreaContextMenuImage';
        },
      },
      translateModeEnabled: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      isSearchboxFocused: Boolean,
      areLanguagePickersOpen: Boolean,
    };
  }

  declare private showTranslateContextMenuItem: boolean;
  declare private showSelectedTextContextMenu: boolean;
  declare private showSelectedRegionContextMenu: boolean;
  declare private showDetectedTextContextMenuOptions: boolean;
  // Location at which to show the context menus.
  declare private selectedTextContextMenuX: number;
  declare private selectedTextContextMenuY: number;
  declare private selectedRegionContextMenuX: number;
  declare private selectedRegionContextMenuY: number;
  declare private selectedRegionContextMenuHorizontalStyle: string;
  declare private selectedRegionContextMenuVerticalStyle: string;
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
  private isPointerInsideButton = false;
  declare private enableCopyAsImage: boolean;
  declare private enableSaveAsImage: boolean;
  declare private suppressCopyAndSaveAsImage: boolean;

  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();

  // Whether the close button used metric was recorded in this session.
  private closeButtonUsedRecorded = false;

  // Whether or not translate mode is enabled. If true, only text should
  // be selectable, and it should be selectable from any point in the
  // overlay.
  declare private translateModeEnabled: boolean;

  protected backgroundImageCanvas(): HTMLCanvasElement {
    return this.$.backgroundImageCanvas;
  }

  protected cursor(): HTMLElement {
    return this.$.cursor;
  }

  protected initialFlashScrim(): HTMLElement {
    return this.$.initialFlashScrim;
  }

  protected overlayShimmerCanvas(): OverlayShimmerCanvasElement {
    return this.$.overlayShimmerCanvas;
  }

  protected postSelectionRenderer(): PostSelectionRendererElement {
    return this.$.postSelectionRenderer;
  }

  protected regionSelectionLayer(): RegionSelectionElement {
    return this.$.regionSelectionLayer;
  }

  protected selectionOverlay(): HTMLElement {
    return this.$.selectionOverlay;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.listenerIds = [
      ...this.listenerIds,
      this.browserProxy.callbackRouter.onCopyCommand.addListener(
          this.onCopyCommand.bind(this)),
      this.browserProxy.callbackRouter.notifyResultsPanelOpened.addListener(
          this.onNotifyResultsPanelOpened.bind(this)),
    ];
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
  }

  protected override shouldIgnoreEvent(event: PointerEvent) {
    if (super.shouldIgnoreEvent(event)) {
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
    return false;
  }

  protected override handlePointerEnter() {
    super.handlePointerEnter();
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

  protected override handleRightClick(event: PointerEvent) {
    if (this.$.textLayer.handleRightClick(event)) {
      return;
    }
    super.handleRightClick(event);
  }

  // LINT.IfChange(CursorOffsetValues)
  // Called on text hover and drag.
  protected override setCursorToText() {
    // Set body cursor style to handle dragging.
    document.body.style.cursor = 'text';
    this.cursorOffsetX = 3;
    this.cursorOffsetY = 8;
    this.style.setProperty(CURSOR_IMG_URL, 'url("text.svg")');
  }

  // Called on region selection drag.
  protected override setCursorToCrosshair() {
    // Set body cursor style to handle dragging.
    document.body.style.cursor = 'crosshair';
    this.cursorOffsetX = 3;
    this.cursorOffsetY = 6;
    this.style.setProperty(CURSOR_IMG_URL, 'url("lens.svg")');
  }

  // Called on object hover.
  protected override setCursorToPointer() {
    // No dragging for objects, so no need to set body cursor style.
    this.cursorOffsetX = 11;
    this.cursorOffsetY = 17;
    this.style.setProperty(CURSOR_IMG_URL, 'url("lens.svg")');
  }

  protected override resetCursor() {
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

  protected override pointerDownHandled() {
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

  override handleGestureStart() {
    this.suppressCopyAndSaveAsImage = false;
    super.handleGestureStart();
    this.dispatchEvent(
        new CustomEvent('selection-started', {bubbles: true, composed: true}));

    // The context menu should have text reset whenever a new selection is
    // started.
    this.detectedTextStartIndex = -1;
    this.detectedTextEndIndex = -1;
    this.showDetectedTextContextMenuOptions = false;

    this.$.textLayer.onSelectionStart();

    if (this.$.postSelectionRenderer.handleGestureStart(this.currentGesture)) {
      this.draggingRespondent = DragFeature.POST_SELECTION;
    } else if (this.$.textLayer.handleGestureStart(this.currentGesture)) {
      // Text is responding to this sequence of gestures.
      this.draggingRespondent = DragFeature.TEXT;
      this.$.postSelectionRenderer.clearSelection();
    }
  }

  protected override handleGestureDrag(event: PointerEvent) {
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

  protected override handleGestureEnd() {
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

  protected override handleGestureCancel() {
    this.$.textLayer.cancelGesture();
    super.handleGestureCancel();
  }

  protected override resizeSelectionCanvases(
      newWidth: number, newHeight: number) {
    this.positionSelectedRegionContextMenu();
    this.$.objectSelectionLayer.setCanvasSizeTo(newWidth, newHeight);
    super.resizeSelectionCanvases(newWidth, newHeight);
  }

  protected override setSidePanelOpened() {
    super.setSidePanelOpened();
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

  protected override onFinishReshowOverlay() {
    recordLensOverlaySelectionCloseButtonShown(INVOCATION_SOURCE);
    super.onFinishReshowOverlay();
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
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-selection-overlay': SelectionOverlayElement;
  }
}

customElements.define(SelectionOverlayElement.is, SelectionOverlayElement);
