// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/lens/region_selection.js';
import '/lens/post_selection_renderer.js';
import '/lens/overlay_border_glow.js';
import '/lens/overlay_shimmer_canvas.js';
import '/strings.m.js';
import './inline_fulfillment_host.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {OverlayBorderGlowElement} from '/lens/overlay_border_glow.js';
import type {OverlayShimmerCanvasElement} from '/lens/overlay_shimmer_canvas.js';
import type {PostSelectionBoundingBox, PostSelectionRendererElement} from '/lens/post_selection_renderer.js';
import type {RegionSelectionElement} from '/lens/region_selection.js';
import {SelectionOverlayBaseLitElement} from '/lens/selection_overlay_base_lit.js';
import {DragFeature, GestureState} from '/lens/selection_utils.js';

import {getCss} from './glic_selection_overlay.css.js';
import {getHtml} from './glic_selection_overlay.html.js';
import type {InlineFulfillmentHostElement} from './inline_fulfillment_host.js';
import {                                   //
  DismissOverlayReason,                    //
  SuggestedActionIcon,                     //
  SuggestedActionsListenerCallbackRouter,  //
} from './selection_overlay.mojom-webui.js';
import type {InteractionOptions, SuggestedAction} from './selection_overlay.mojom-webui.js';
import type {SelectionOverlayBaseHandlerImpl} from './selection_overlay_base_handler_impl.js';

const GLIC_BORDER_GLOW_COLORS: string[] = [
  '#1B6EF3',
  '#0B57D0',
  '#1B6EF3',
  '#7CACF8',
];

/*
 * Element responsible for coordinating selections between the various selection
 * features. This includes:
 *   - Storing state needed to coordinate selections across features
 *   - Listening to mouse/tap events and delegating them to the correct features
 *   - Coordinating animations between the different features
 */
export interface GlicSelectionOverlayElement {
  $: {
    backgroundImageCanvas: HTMLCanvasElement,
    cursor: HTMLElement,
    initialFlashScrim: HTMLElement,
    overlayShimmerCanvas: OverlayShimmerCanvasElement,
    postSelectionRenderer: PostSelectionRendererElement,
    regionSelectionLayer: RegionSelectionElement,
    selectionOverlay: HTMLElement,
  };
}

export class GlicSelectionOverlayElement extends
    SelectionOverlayBaseLitElement {
  static get is() {
    return 'glic-selection-overlay';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      ...super.properties,
      screenshotDataUri: {type: String},
      enableRegionSelectedGlow: {type: Boolean},
      enableBorderGlow: {type: Boolean},
      disableShimmer: {type: Boolean},
      showFloatingPrompt: {type: Boolean},
      promptAboveSelection: {type: Boolean},
      floatingPromptStyle: {type: String},
      activeSelection: {type: Object},
      suggestedActions: {type: Array},
      enableSelectionOverlayPrompt: {type: Boolean},
      enableSelectionOverlayPromptBox: {type: Boolean},
      enableSelectionOverlayDogfoodFeedback: {type: Boolean},
      hideHandles: {type: Boolean},
      disableMultiSelect: {type: Boolean, reflect: true},
      skipIntroAnimation: {type: Boolean, reflect: true},
      hasPromptText: {type: Boolean},
      sidePanelOpen: {type: Boolean, reflect: true},
      pageContextReceived: {type: Boolean},
    };
  }

  accessor screenshotDataUri: string = '';
  accessor enableRegionSelectedGlow: boolean = true;
  override accessor enableBorderGlow: boolean = true;
  override accessor disableShimmer: boolean = false;
  accessor showFloatingPrompt: boolean = false;
  accessor promptAboveSelection: boolean = false;
  accessor floatingPromptStyle: string = '';
  accessor activeSelection: PostSelectionBoundingBox|null = null;
  accessor suggestedActions: SuggestedAction[] = [];
  accessor enableSelectionOverlayPrompt: boolean =
      loadTimeData.getBoolean('enableSelectionOverlayPrompt');
  accessor enableSelectionOverlayPromptBox: boolean =
      loadTimeData.getBoolean('enableSelectionOverlayPromptBox');
  accessor enableSelectionOverlayDogfoodFeedback: boolean =
      loadTimeData.getBoolean('enableSelectionOverlayDogfoodFeedback');
  accessor hideHandles: boolean = false;
  accessor disableMultiSelect: boolean = false;
  accessor skipIntroAnimation: boolean = false;
  // Whether the prompt input has non-whitespace text, which shows the submit
  // button.
  accessor hasPromptText: boolean = false;
  // Whether the Glic side panel is open for this tab: showing, or backgrounded
  // and showing again once the tab is in front. Kept live by the browser for
  // the length of the session. Separate from the base class's
  // `sidePanelOpened`, which applies the Lens side panel margins; the Glic
  // overlay is tab-scoped and never applies them.
  accessor sidePanelOpen: boolean = false;
  // Whether the browser has the page content. Until then a prompt can't be
  // submitted.
  accessor pageContextReceived: boolean = false;
  // Whether the current gesture started with the right button.
  private isRightButtonGesture_: boolean = false;
  // Number of selected regions, as last sent by the browser.
  private regionCount_: number = 0;

  override connectedCallback() {
    super.connectedCallback();
    const handlerImpl = this.baseHandler as SelectionOverlayBaseHandlerImpl;
    this.listenerIds.push(handlerImpl.addSetInteractionOptionsListener(
        (options: InteractionOptions) => {
          this.hideHandles = options.hideHandles;
          this.disableMultiSelect = options.disableMultiSelect;
          this.skipIntroAnimation = options.skipIntroAnimation;
        }));
    this.listenerIds.push(
        handlerImpl.addSetSidePanelOpenListener((open: boolean) => {
          this.sidePanelOpen = open;
        }));
    this.listenerIds.push(handlerImpl.addPageContextReceivedListener(() => {
      this.pageContextReceived = true;
    }));
    this.listenerIds.push(this.baseHandler.addMultiRegionSelectionListener(
        (regions) => this.onRegionCountChanged_(regions.length)));
  }

  // The prompt and its suggestions are for a single area. With more, hide
  // them; bring them back once there is one again.
  private onRegionCountChanged_(count: number) {
    const wasMultiple = this.hasMultipleRegions_();
    this.regionCount_ = count;
    if (this.hasMultipleRegions_()) {
      this.showFloatingPrompt = false;
      this.clearSuggestedActions_();
    } else if (wasMultiple && count === 1) {
      this.fetchSuggestedActions();
      this.updateFloatingPromptPosition();
    }
  }

  private hasMultipleRegions_(): boolean {
    return this.regionCount_ > 1;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.floatingPromptResizeObserver_.disconnect();
  }

  override firstUpdated() {
    super.firstUpdated();
    GLIC_BORDER_GLOW_COLORS.forEach((color, index) => {
      this.style.setProperty(`--overlay-border-glow-color-${index + 1}`, color);
    });
    this.updateThemeColors();
    this.resetCursor();

    this.eventTracker_.add(
        document, 'post-selection-updated',
        (e: CustomEvent<PostSelectionBoundingBox>) => {
          // `PostSelectionRendererElement.clearSelection()` reports a cleared
          // selection as an update with zero size.
          if (e.detail.width === 0 && e.detail.height === 0) {
            this.skipIntroAnimation = false;
          }
          const prev = this.activeSelection;
          const selectionChanged = !prev ||
              Math.abs(prev.top - e.detail.top) > 1e-5 ||
              Math.abs(prev.left - e.detail.left) > 1e-5 ||
              Math.abs(prev.width - e.detail.width) > 1e-5 ||
              Math.abs(prev.height - e.detail.height) > 1e-5;
          this.activeSelection = e.detail;
          if (this.currentGesture?.state === GestureState.NOT_STARTED ||
              this.currentGesture?.state === undefined) {
            // Activate the dark scrim on the region selection layer, which
            // normally only activates at the end of a manual drag gesture.
            this.selectionElements.regionSelectionLayer
                .handlePostSelectionDragGestureEnd();
            this.updateFloatingPromptPosition();
            if (selectionChanged) {
              this.fetchSuggestedActions();
            }
          }
        });

    // TODO(b/501134201): 'post-selection-cleared' is never dispatched. Move
    // this cleanup into the zero-size branch of the 'post-selection-updated'
    // listener above and return early there, so that a cleared selection does
    // not re-activate the scrim. Then remove this listener.
    this.eventTracker_.add(document, 'post-selection-cleared', () => {
      this.activeSelection = null;
      this.showFloatingPrompt = false;
      this.clearInlineFulfillment_();
    });

    if (this.enableSelectionOverlayPrompt ||
        this.enableSelectionOverlayPromptBox) {
      this.fetchSuggestedActions();
    }

    this.eventTracker_.add(window, 'resize', () => {
      if (this.showFloatingPrompt) {
        this.updateFloatingPromptPosition();
      }
    });

    // A context menu would interrupt a right drag. Keep it in the prompt
    // input for copy and paste.
    this.eventTracker_.add(this, 'contextmenu', (e: Event) => {
      if (this.dragsOnRightButton() &&
          !(e.composedPath()[0] instanceof HTMLInputElement)) {
        e.preventDefault();
      }
    });

    const container =
        this.shadowRoot.querySelector<HTMLElement>('#floatingPromptContainer');
    if (container) {
      this.floatingPromptResizeObserver_.observe(container);
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('skipIntroAnimation') &&
        this.skipIntroAnimation) {
      this.selectionElements.regionSelectionLayer
          .handlePostSelectionDragGestureEnd();
    }
    if (changedProperties.has('showFloatingPrompt')) {
      this.focusPromptInput_();
    }
  }

  override get selectionElements() {
    return {
      backgroundImageCanvas: this.$.backgroundImageCanvas,
      cursor: this.$.cursor,
      initialFlashScrim: this.$.initialFlashScrim,
      overlayShimmerCanvas: this.$.overlayShimmerCanvas,
      postSelectionRenderer: this.$.postSelectionRenderer,
      regionSelectionLayer: this.$.regionSelectionLayer,
      selectionOverlay: this.$.selectionOverlay,
    };
  }

  // Overridden to log container and screenshot dimensions for diagnosing
  // unexpected insets / margins (b/512915349). Unlike Lens Overlay, gPointer
  // is tab-scoped and should never apply side panel margins or insets (unless
  // the user resizes the browser window).
  protected override updateCanvasSize(
      containerWidth: number, containerHeight: number) {
    super.updateCanvasSize(containerWidth, containerHeight);

    const canvas = this.selectionElements.backgroundImageCanvas;
    const screenshotWidth = canvas.width;
    const screenshotHeight = canvas.height;
    const dpr = window.devicePixelRatio;
    const diffW = Math.abs(containerWidth - (screenshotWidth / dpr));
    const diffH = Math.abs(containerHeight - (screenshotHeight / dpr));
    const doesScreenshotFillContainer = diffW <= 2 && diffH <= 2;
    const shouldApplyMargins =
        !doesScreenshotFillContainer || this.sidePanelOpened;

    // Diagnostics for b/512915349: Check the logged values against each
    // hypothesis:
    //
    // Hypothesis 1 (Mojo/State): sidePanelOpened=true
    // Proves: isSidePanelOpen was erroneously passed as true via Mojo or
    // setSidePanelOpened() was called, unconditionally applying margins.
    //
    // Hypothesis 2 (Fractional DPI): 2px < diffW/diffH <= 4px
    // Why 2px-4px: SCREENSHOT_RESIZE_TOLERANCE_PIXELS is 2px. Converting
    // physical pixels to DIPs (physical / devicePixelRatio) on
    // fractional/Retina displays introduces floating-point and rounding
    // jitter across layers (Blink layout vs compositor surface vs WebUI).
    // Differences between 2.01px and ~4px prove subpixel rounding noise
    // barely crossed the strict 2px threshold, rather than a real layout
    // shift.
    //
    // Hypothesis 3 (Late Resize): diffW >> 5px or changing container dims
    // Why >> 5px: Differentiates real layout changes from rounding noise
    // (<=4px). A side panel animation frame shifts layout by 20-60px per
    // frame (total panel is ~400px), and window resizes shift by
    // dozens/hundreds of pixels. Diffs >> 5px prove container resized AFTER
    // the screenshot was captured.
    //
    // Hypothesis 4 (Top Chrome/Toolbar): diffW <= 2px, diffH >> 2px (e.g.
    // 36-72px) Proves: Height mismatch from UI elements (e.g. download shelf,
    // bookmark bar, or tabstrip bounds offset in C++ capture).
    if (shouldApplyMargins) {
      console.error(
          `[SelectionOverlayDebug] ` +
          `container=${containerWidth}x${containerHeight}, ` +
          `screenshot=${screenshotWidth}x${screenshotHeight}, ` +
          `dpr=${dpr}, ` +
          `diffW=${diffW.toFixed(2)}, diffH=${diffH.toFixed(2)}, ` +
          `doesScreenshotFillContainer=${doesScreenshotFillContainer}, ` +
          `sidePanelOpened=${this.sidePanelOpened}, ` +
          `isResized=${this.isResized}, ` +
          `shouldApplyMargins=${shouldApplyMargins}`);
    }
  }

  // A prewarmed overlay is on screen while the screenshot is still being
  // captured. Until it is shown, ignore input so a click on the live page can't
  // draw a region or dismiss the overlay.
  protected override shouldIgnoreEvent(event: PointerEvent): boolean {
    return !this.isScreenshotRendered || super.shouldIgnoreEvent(event);
  }

  // Repositions the prompt when its size changes, e.g. when a card is added
  // or its content grows, so it doesn't overflow the viewport.
  private floatingPromptResizeObserver_ = new ResizeObserver(() => {
    if (this.showFloatingPrompt) {
      this.updateFloatingPromptPosition();
    }
  });

  private updateThemeColors() {
    const selectionColor = this.getSelectionElementColor();
    if (selectionColor) {
      this.style.setProperty('--color-selection-element', selectionColor);
    }
    const primaryColor = this.getPrimaryColor();
    if (primaryColor) {
      this.style.setProperty('--color-primary', primaryColor);
    }
  }

  // Only rendered when the prompt is enabled.
  private get inlineFulfillmentHost_(): InlineFulfillmentHostElement|null {
    return this.shadowRoot.querySelector<InlineFulfillmentHostElement>(
        '#inlineFulfillmentHost');
  }

  // The action whose card is showing, if any.
  private shownInlineAction_: SuggestedAction|null = null;

  // Height of `.action-chips-row` before the inline fulfillment card replaced
  // it, used to keep the card's top-left corner at the row's top-left corner.
  private replacedChipsRowHeight_ = 0;

  private clearInlineFulfillment_() {
    this.shownInlineAction_ = null;
    this.inlineFulfillmentHost_?.clear();
  }

  private suggestedActionsListenerRouter_:
      SuggestedActionsListenerCallbackRouter|null = null;

  private clearSuggestedActions_() {
    this.suggestedActionsListenerRouter_?.$.close();
    this.suggestedActionsListenerRouter_ = null;
    this.suggestedActions = [];
    // A card belongs to the previous selection's actions.
    this.clearInlineFulfillment_();
  }

  private fetchSuggestedActions() {
    if (!this.enableSelectionOverlayPrompt &&
        !this.enableSelectionOverlayPromptBox) {
      return;
    }
    this.clearSuggestedActions_();
    if (this.hasMultipleRegions_()) {
      return;
    }
    this.suggestedActionsListenerRouter_ =
        new SuggestedActionsListenerCallbackRouter();
    this.suggestedActionsListenerRouter_.onSuggestedActionsAvailable
        .addListener(async (actions: SuggestedAction[]) => {
          const combined = [...this.suggestedActions, ...actions];
          const askGeminiActions = combined.filter(
              action => action.title.toLowerCase() === 'ask gemini');
          const otherActions = combined.filter(
              action => action.title.toLowerCase() !== 'ask gemini');
          this.suggestedActions = [...otherActions, ...askGeminiActions];
          await this.updateComplete;
          if (this.showFloatingPrompt) {
            this.updateFloatingPromptPosition();
          }
        });
    const handlerImpl = this.baseHandler as SelectionOverlayBaseHandlerImpl;
    handlerImpl.getSuggestedActions(
        this.suggestedActionsListenerRouter_.$.bindNewPipeAndPassRemote());
  }

  protected override get defaultCursorIconUrl() {
    return 'url("/glic_region_selection_cursor_icon.svg")';
  }

  private updateFloatingPromptPosition() {
    if (!this.enableSelectionOverlayPrompt &&
        !this.enableSelectionOverlayPromptBox) {
      this.showFloatingPrompt = false;
      return;
    }

    if (!this.selectionElements.postSelectionRenderer.hasSelection() ||
        this.hasMultipleRegions_()) {
      this.showFloatingPrompt = false;
      return;
    }

    const bounds = this.activeSelection;
    if (!bounds || (bounds.width === 0 && bounds.height === 0)) {
      this.showFloatingPrompt = false;
      return;
    }

    const overlayRect = this.selectionOverlayRect;
    const selLeft = overlayRect.left + bounds.left * overlayRect.width;
    const selTop = overlayRect.top + bounds.top * overlayRect.height;
    const selWidth = bounds.width * overlayRect.width;
    const selHeight = bounds.height * overlayRect.height;
    const selBottom = selTop + selHeight;
    const selRight = selLeft + selWidth;

    const margin = 16;
    const viewportWidth = window.innerWidth;
    const viewportHeight = window.innerHeight;
    const maxAvailableWidth = Math.max(0, viewportWidth - 2 * margin);

    const computeStyle = (width: number, height: number): string => {
      if (this.enableSelectionOverlayPromptBox) {
        const isRtl = getComputedStyle(this).direction === 'rtl';
        // Sufficient space requires fitting the input box (320px wide, 44px
        // tall) plus two 1-line contextual suggestions (32px tall each) with
        // 6px gaps (44 + 6 + 32 + 6 + 32 = 120px). The inline fulfillment host
        // prefers 380px width but can shrink down to 320px to fit the available
        // space at the chosen position without moving.
        const minWidth = 320;
        const minHeight = 120;

        const fitsVertically = (reqTop: number): boolean => {
          return reqTop >= margin &&
              reqTop + minHeight <= viewportHeight - margin;
        };

        const leftAligned =
            (anchorLeft: number,
             limitRight: number = viewportWidth - margin) => {
              const availableWidth = limitRight - anchorLeft;
              const maxWidth = Math.max(minWidth, availableWidth);
              return {
                left: anchorLeft,
                maxWidth,
                validX: anchorLeft >= margin && availableWidth >= minWidth,
              };
            };

        const rightAligned =
            (anchorRight: number, limitLeft: number = margin) => {
              const availableWidth = anchorRight - limitLeft;
              const maxWidth = Math.max(minWidth, availableWidth);
              return {
                left: anchorRight - Math.min(width, maxWidth),
                maxWidth,
                validX: anchorRight <= viewportWidth - margin &&
                    availableWidth >= minWidth,
              };
            };

        const alignStart = {
          ...(isRtl ? rightAligned(selRight) : leftAligned(selLeft)),
          alignItems: 'flex-start',
        };
        const alignEnd = {
          ...(isRtl ? leftAligned(selLeft) : rightAligned(selRight)),
          alignItems: 'flex-end',
        };
        const afterSelection = {
          ...(isRtl ? rightAligned(selLeft - margin) :
                      leftAligned(selRight + margin)),
          alignItems: 'flex-start',
        };
        const beforeSelection = {
          ...(isRtl ? leftAligned(selRight + margin) :
                      rightAligned(selLeft - margin)),
          alignItems: 'flex-end',
        };
        const insideSelection = {
          ...(isRtl ?
                  rightAligned(
                      selRight - margin, Math.max(selLeft + margin, margin)) :
                  leftAligned(
                      selLeft + margin,
                      Math.min(selRight - margin, viewportWidth - margin))),
          alignItems: 'flex-start',
        };
        const windowCorner = {
          ...(isRtl ? rightAligned(viewportWidth - margin) :
                      leftAligned(margin)),
          alignItems: 'flex-start',
        };

        const candidates = [
          // 1. Below selected area, with both left edges aligned (right for
          // RTL).
          {
            ...alignStart,
            top: selBottom + margin,
            above: false,
            valid: alignStart.validX && fitsVertically(selBottom + margin),
          },
          // 2. Below selected area, with both right edges aligned (left for
          // RTL).
          {
            ...alignEnd,
            top: selBottom + margin,
            above: false,
            valid: alignEnd.validX && fitsVertically(selBottom + margin),
          },
          // 3. Above selected area, with both left edges aligned (right for
          // RTL).
          {
            ...alignStart,
            top: selTop - margin - height,
            above: true,
            valid: alignStart.validX &&
                fitsVertically(selTop - margin - minHeight),
          },
          // 4. Above selected area, with both right edges aligned (left for
          // RTL).
          {
            ...alignEnd,
            top: selTop - margin - height,
            above: true,
            valid:
                alignEnd.validX && fitsVertically(selTop - margin - minHeight),
          },
          // 5. To the right of selected area (left for RTL), with both top
          // edges aligned.
          {
            ...afterSelection,
            top: selTop,
            above: false,
            valid: afterSelection.validX && fitsVertically(selTop),
          },
          // 6. To the left of selected area (right for RTL), with both top
          // edges aligned.
          {
            ...beforeSelection,
            top: selTop,
            above: false,
            valid: beforeSelection.validX && fitsVertically(selTop),
          },
          // 7. Within the selected area, with top-left corners aligned
          // (top-right for RTL).
          {
            ...insideSelection,
            top: selTop + margin,
            above: false,
            valid: insideSelection.validX && fitsVertically(selTop + margin) &&
                selTop + margin + minHeight <= selBottom - margin,
          },
        ];

        const chosen = candidates.find(c => c.valid) ?? {
          // 8. In the top-left corner of the window (top-right for RTL).
          ...windowCorner,
          top: margin,
          above: false,
        };

        this.promptAboveSelection = chosen.above;
        return `left: ${chosen.left}px; top: ${chosen.top}px; max-width: ${
            chosen.maxWidth}px; align-items: ${chosen.alignItems};`;
      }

      const effectiveWidth = Math.min(width, maxAvailableWidth);
      this.promptAboveSelection = false;

      // When the prompt box is not shown, anchor the bottom-left of the row at
      // the region's top-right corner (selRight, selTop). When an inline
      // fulfillment card replaces the row, keep the card's top-left corner at
      // the row's top-left corner so it expands downward. The row is positioned
      // by its left edge so the chips grow out of the spark's spot.
      const clampedLeft = Math.max(
          margin, Math.min(selRight, viewportWidth - effectiveWidth - margin));

      const idealTop = selTop -
          (this.shownInlineAction_ ? this.replacedChipsRowHeight_ : height);
      const targetTop = Math.max(
          margin, Math.min(idealTop, viewportHeight - height - margin));
      return `left: ${clampedLeft}px; top: ${targetTop}px;`;
    };

    const container =
        this.shadowRoot.querySelector<HTMLElement>('#floatingPromptContainer');
    let promptWidth = this.enableSelectionOverlayPromptBox ? 320 : 32;
    let promptHeight = this.enableSelectionOverlayPromptBox ? 44 : 32;

    if (container) {
      const rect = container.getBoundingClientRect();
      if (rect.width > 0) {
        promptWidth = rect.width;
        promptHeight = rect.height;
      }
    }

    this.floatingPromptStyle = computeStyle(promptWidth, promptHeight);
    this.showFloatingPrompt = true;

    const refinePosition = () => {
      if (!this.showFloatingPrompt) {
        return;
      }
      const currentContainer = this.shadowRoot.querySelector<HTMLElement>(
          '#floatingPromptContainer');
      if (currentContainer) {
        const newRect = currentContainer.getBoundingClientRect();
        if (newRect.width > 0 &&
            (Math.abs(newRect.width - promptWidth) > 1 ||
             Math.abs(newRect.height - promptHeight) > 1)) {
          promptWidth = newRect.width;
          promptHeight = newRect.height;
          this.floatingPromptStyle =
              computeStyle(newRect.width, newRect.height);
        }
      }
    };

    // Refine position once Lit renders the unhidden container and in the next
    // animation frame after layout resolves.
    this.updateComplete.then(refinePosition);
    requestAnimationFrame(refinePosition);
  }

  // With the prompt box, a right drag adds an area and a left drag replaces
  // it.
  protected override dragsOnRightButton(): boolean {
    return this.enableSelectionOverlayPromptBox && !this.disableMultiSelect;
  }

  protected override onPointerDown(event: PointerEvent) {
    this.isRightButtonGesture_ = event.button === 2;
    super.onPointerDown(event);
  }

  override handleGestureStart() {
    super.handleGestureStart();
    this.showFloatingPrompt = false;
    // A right drag always draws a new area, even over an existing one.
    if (this.isRightButtonGesture_) {
      return;
    }
    if (this.selectionElements.postSelectionRenderer.handleGestureStart(
            this.currentGesture)) {
      this.draggingRespondent = DragFeature.POST_SELECTION;
    }
  }

  protected override handleGestureDrag(event: PointerEvent) {
    assert(this.currentGesture.state === GestureState.DRAGGING);
    this.showFloatingPrompt = false;
    // Capture pointer events so gestures still work if the users pointer
    // leaves the selection overlay div. Pointer capture is implicitly
    // released after pointerup or pointercancel events.
    this.setPointerCapture(event.pointerId);

    if (this.draggingRespondent === DragFeature.POST_SELECTION) {
      this.selectionElements.postSelectionRenderer.handleGestureDrag(
          this.currentGesture);
      return;
    }

    // If no one is responding to the drag yet, then let the region selection
    // layer respond.
    if (this.draggingRespondent === DragFeature.NONE) {
      // Disable region selection on click.
      // TODO(b/565871325): Allow adding regions while Ctrl or Cmd is held, here
      // and in `handleGestureEnd()`.
      if (this.disableMultiSelect) {
        return;
      }
      this.setCursorToCrosshair();
      this.draggingRespondent = DragFeature.MANUAL_REGION;

      // With the prompt box, only a right drag with the side panel open adds
      // an area. Otherwise keep the active id so the drag replaces it.
      if (!this.enableSelectionOverlayPromptBox ||
          (this.isRightButtonGesture_ && this.sidePanelOpen)) {
        this.activeRegionId = '';
        this.baseHandler.activeRegionId = '';
      }
      this.selectionElements.postSelectionRenderer.clearSelection();

      // TODO(crbug.com/421002691): follow the convention where the layer
      // should return true if its handling the gesture, and draggingRespondent
      // should be updated. Currently used to trigger the fade in of the
      // darkened scrim.
      this.selectionElements.regionSelectionLayer.handleGestureStart();
    }

    if (this.draggingRespondent === DragFeature.MANUAL_REGION) {
      this.selectionElements.regionSelectionLayer.handleGestureDrag(
          this.currentGesture);
    }
  }

  protected override handleGestureEnd() {
    if (this.disableMultiSelect &&
        this.draggingRespondent === DragFeature.NONE) {
      (this.baseHandler as SelectionOverlayBaseHandlerImpl)
          .dismissOverlay(DismissOverlayReason.kBackgroundClick);
      return;
    }

    // Allow proper feature to respond to the tap/drag event.
    switch (this.currentGesture.state) {
      case GestureState.DRAGGING:

        // Drag has finished. Let the features respond to the end of a drag.
        if (this.draggingRespondent === DragFeature.MANUAL_REGION) {
          this.selectionElements.regionSelectionLayer.handleGestureEnd(
              this.currentGesture);
        } else if (this.draggingRespondent === DragFeature.POST_SELECTION) {
          this.selectionElements.postSelectionRenderer.handleGestureEnd();
          // Fade out scrim which is currently being managed by region selection
          // TODO(crbug.com/420998632): move scrim out to its own component
          this.selectionElements.regionSelectionLayer
              .handlePostSelectionDragGestureEnd();
        }
        this.fetchSuggestedActions();
        break;
      case GestureState.STARTING:
        if (this.draggingRespondent === DragFeature.NONE) {
          // With the prompt box, a click doesn't select an area around it. A
          // left click closes the overlay once there is a region; otherwise it
          // does nothing.
          if (this.enableSelectionOverlayPromptBox) {
            if (this.activeRegionId && !this.isRightButtonGesture_) {
              (this.baseHandler as SelectionOverlayBaseHandlerImpl)
                  .dismissOverlay(DismissOverlayReason.kBackgroundClick);
            }
            break;
          }
          this.baseHandler.activeRegionId = '';
          this.selectionElements.regionSelectionLayer.handleGestureEnd(
              this.currentGesture);
        }
        break;
      default:  // Other states are invalid and ignored.
        break;
    }

    this.resetCursor();
    this.updateFloatingPromptPosition();
  }

  protected onPromptInput(event: Event) {
    const input = event.target as HTMLInputElement;
    this.hasPromptText = input.value.trim().length > 0;
  }

  protected onInputKeydown(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      this.submitPromptInput_();
    } else if (event.key === 'Escape') {
      (this.baseHandler as SelectionOverlayBaseHandlerImpl)
          .dismissOverlay(DismissOverlayReason.kCloseButton);
    }
  }

  protected onSubmitPromptClick(event: Event) {
    event.stopPropagation();
    this.submitPromptInput_();
  }

  protected onSearchboxPillClick() {
    this.focusPromptInput_();
  }

  private focusPromptInput_() {
    this.shadowRoot.querySelector<HTMLInputElement>('#promptInput')?.focus();
  }

  protected getSubmitPromptTitle(): string {
    return loadTimeData.getString(
        this.pageContextReceived ? 'submitPrompt' : 'fetchingPageContent');
  }

  private submitPromptInput_() {
    if (!this.pageContextReceived) {
      return;
    }
    const input =
        this.shadowRoot.querySelector<HTMLInputElement>('#promptInput');
    const prompt = input?.value.trim() ?? '';
    if (prompt.length > 0) {
      this.submitPrompt(prompt);
    }
  }

  protected async onSuggestedActionClick(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const action = this.suggestedActions[index];
    if (!action) {
      return;
    }
    const handler = this.baseHandler as SelectionOverlayBaseHandlerImpl;
    const inlineFulfillment = action.action.inlineFulfillment;
    if (!inlineFulfillment) {
      handler.executeSuggestedAction(action.id, null);
      return;
    }
    const host = this.inlineFulfillmentHost_;
    // Its card is already showing. Executing it again would send the browser
    // a second channel for the same action.
    if (!host || action === this.shownInlineAction_) {
      return;
    }
    // TODO(liuwilliam): We should consider moving this to pointerdown to save
    // ~200ms of click time.
    //
    // `show()` sends the channel only if the card loaded and no newer click or
    // selection replaced it. Otherwise nothing is sent, since the browser
    // rejects an inline action without a channel.
    //
    // The state is updated only once the card shows: if it doesn't, e.g. the
    // user declined its consent prompt, the chips (or the current card) stay,
    // and the chip can be clicked again.
    const chipsRowHeight = this.shownInlineAction_ ?
        this.replacedChipsRowHeight_ :
        this.shadowRoot.querySelector<HTMLElement>(
                           '.action-chips-row')!.offsetHeight;
    if (await host.show(inlineFulfillment.resourceName, channel => {
          handler.executeSuggestedAction(action.id, channel);
        })) {
      this.replacedChipsRowHeight_ = chipsRowHeight;
      this.shownInlineAction_ = action;
    }
  }

  protected onInlineFulfillmentCloseClick() {
    // Closes the overlay without the hidden toast, like a background click.
    // TODO(b/546644969): Reorganize overlay close reasons once the UX is final.
    (this.baseHandler as SelectionOverlayBaseHandlerImpl)
        .dismissOverlay(DismissOverlayReason.kBackgroundClick);
  }

  protected submitPrompt(prompt: string) {
    (this.baseHandler as SelectionOverlayBaseHandlerImpl).submitPrompt(prompt);
  }

  protected onPromptPointerdown(event: PointerEvent) {
    event.stopPropagation();
  }

  protected onSettingsClick(event: Event) {
    (this.baseHandler as SelectionOverlayBaseHandlerImpl).openSettings();
    event.stopPropagation();
  }

  protected onDogfoodFeedbackClick(event: Event) {
    (this.baseHandler as SelectionOverlayBaseHandlerImpl).openDogfoodFeedback();
    event.stopPropagation();
  }

  protected getActionIconClass(icon: SuggestedActionIcon): string {
    switch (icon) {
      case SuggestedActionIcon.kSpark:
        return 'spark';
      case SuggestedActionIcon.kExplain:
        return 'explain';
      case SuggestedActionIcon.kNone:
      default:
        return '';
    }
  }

  override getOverlayBorderGlow(): OverlayBorderGlowElement {
    return this.shadowRoot.querySelector<OverlayBorderGlowElement>(
        '#overlayBorderGlow')!;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'glic-selection-overlay': GlicSelectionOverlayElement;
  }
}

customElements.define(
    GlicSelectionOverlayElement.is, GlicSelectionOverlayElement);
