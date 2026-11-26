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

import {PromptEndpointId} from './action.mojom-webui.js';
import {getCss} from './glic_selection_overlay.css.js';
import {getHtml} from './glic_selection_overlay.html.js';
import type {InlineFulfillmentHostElement} from './inline_fulfillment_host.js';
import {                                   //
  DismissOverlayReason,                    //
  SuggestedActionIcon,                     //
  SuggestedActionsListenerCallbackRouter,  //
} from './selection_overlay.mojom-webui.js';
import type {InteractionOptions, PromptEndpointInfo, SuggestedAction} from './selection_overlay.mojom-webui.js';
import type {SelectionOverlayBaseHandlerImpl} from './selection_overlay_base_handler_impl.js';

const GLIC_BORDER_GLOW_COLORS: string[] = [
  '#1B6EF3',
  '#0B57D0',
  '#1B6EF3',
  '#7CACF8',
];

const ENDPOINT_SWITCH_DURATION_MS = 420;
const ENDPOINT_SWITCH_EASING = 'cubic-bezier(0.22, 1, 0.36, 1)';
const ENDPOINT_CIRCLE_SIZE_PX = 44;
const ENDPOINT_ROW_GAP_PX = 6;

function createPillGhost(pill: HTMLElement): HTMLElement {
  const ghost = pill.cloneNode(true) as HTMLElement;
  ghost.querySelector('#endpointLiveRegion')?.remove();
  const realInput = pill.querySelector<HTMLInputElement>('#promptInput');
  const ghostInput = ghost.querySelector<HTMLInputElement>('.prompt-input');
  if (realInput && ghostInput) {
    ghostInput.value = realInput.value;
    ghostInput.readOnly = true;
    ghostInput.tabIndex = -1;
  }
  ghost.removeAttribute('id');
  ghost.querySelectorAll('[id]').forEach(el => el.removeAttribute('id'));
  ghost.setAttribute('aria-hidden', 'true');
  ghost.inert = true;
  ghost.classList.add('searchbox-pill-ghost');
  return ghost;
}

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
      showFloatingPrompt: {type: Boolean},
      promptAboveSelection: {type: Boolean},
      floatingPromptStyle: {type: String},
      activeSelection: {type: Object},
      suggestedActions: {type: Array},
      enableSelectionOverlayPrompt: {type: Boolean},
      enableSelectionOverlayPromptBox: {type: Boolean},
      enableSelectionOverlayDogfoodFeedback: {type: Boolean},
      hideHandles: {type: Boolean},
      hasSelection: {type: Boolean, reflect: true},
      skipIntroAnimation: {type: Boolean, reflect: true},
      hasPromptText: {type: Boolean},
      sidePanelOpen: {type: Boolean, reflect: true},
      pageContextReceived: {type: Boolean},
      promptEndpoints: {type: Array},
      activeEndpointIndex: {type: Number},
      endpointLiveAnnouncement: {type: String},
    };
  }

  accessor screenshotDataUri: string = '';
  accessor enableRegionSelectedGlow: boolean = true;
  override readonly enableBorderGlow: boolean = true;
  override readonly disableShimmer: boolean = false;
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
  accessor hasSelection: boolean = false;
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
  // Ordered list of eligible prompt endpoints configured by the browser.
  accessor promptEndpoints: PromptEndpointInfo[] = [];
  accessor activeEndpointIndex: number = 0;
  accessor endpointLiveAnnouncement: string = '';
  // The platform's usual page scrollbar width, which is part of the
  // screenshot. The chips keep their margin from it.
  private readonly pageScrollbarWidth_: number =
      loadTimeData.getInteger('pageScrollbarWidth');
  // Whether the current gesture started with the right button.
  private isRightButtonGesture_: boolean = false;
  // Number of selected regions, as last sent by the browser.
  private regionCount_: number = 0;
  // Tracks the active endpoint switch animation so rapid switches or overlay
  // teardown cleanly cancel and reset transient DOM state.
  private switchAnimationToken_: number = 0;
  private switchAnimations_: Animation[] = [];
  private switchCleanup_: (() => void)|null = null;
  private switchCleanupTimer_: number = 0;
  // Last `max-width` (in px) applied to `#floatingPromptContainer`, used to
  // avoid changing container width constraints synchronously inside
  // `floatingPromptResizeObserver_`.
  private floatingPromptMaxWidth_: number = 0;
  private floatingPromptResizeRafId_: number = 0;

  override connectedCallback() {
    super.connectedCallback();
    const handlerImpl = this.baseHandler as SelectionOverlayBaseHandlerImpl;
    this.listenerIds.push(handlerImpl.addSetInteractionOptionsListener(
        (options: InteractionOptions) => {
          this.hideHandles = options.hideHandles;
          this.skipIntroAnimation = options.skipIntroAnimation;
        }));
    this.listenerIds.push(handlerImpl.addSetPromptEndpointsListener(
        (endpoints: PromptEndpointInfo[],
         defaultEndpointId: PromptEndpointId) => {
          this.finishSwitchAnimation_();
          const prevActiveId = this.getActiveEndpoint_()?.id ?? null;
          this.promptEndpoints = endpoints;
          const defaultIdx =
              endpoints.findIndex(e => e.id === defaultEndpointId);
          this.activeEndpointIndex = defaultIdx >= 0 ? defaultIdx : 0;
          const nextActive = this.getActiveEndpoint_();
          this.endpointLiveAnnouncement = prevActiveId !== null && nextActive &&
                  nextActive.id !== prevActiveId ?
              nextActive.name :
              '';
          if (this.showFloatingPrompt &&
              this.selectionElements.postSelectionRenderer?.hasSelection()) {
            this.updateFloatingPromptPosition();
          }
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
    if (count === 0) {
      this.activeSelection = null;
    }
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

  private hasSelection_(): boolean {
    return Boolean(this.activeRegionId) ||
        (this.activeSelection !== null &&
         (this.activeSelection.width > 0 || this.activeSelection.height > 0));
  }

  // When the side panel is closed, only one region can be selected.
  private canSelectRegion_(): boolean {
    return this.sidePanelOpen || !this.hasSelection_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.finishSwitchAnimation_();
    this.cancelDeferredFloatingPromptPositionUpdate_();
    this.floatingPromptResizeObserver_.disconnect();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    this.hasSelection = this.hasSelection_();
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
      if (this.enableSelectionOverlayPromptBox &&
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
      if (this.showFloatingPrompt) {
        this.focusPromptInput_();
      } else {
        this.finishSwitchAnimation_();
      }
    }
    if (changedProperties.has('enableSelectionOverlayPromptBox') &&
        !this.enableSelectionOverlayPromptBox) {
      this.finishSwitchAnimation_();
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
      this.updateFloatingPromptPosition(/*fromResizeObserver=*/ true);
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

  private deferFloatingPromptPositionUpdate_() {
    if (!this.floatingPromptResizeRafId_) {
      this.floatingPromptResizeRafId_ = requestAnimationFrame(() => {
        this.floatingPromptResizeRafId_ = 0;
        if (this.showFloatingPrompt) {
          this.updateFloatingPromptPosition();
        }
      });
    }
  }

  private cancelDeferredFloatingPromptPositionUpdate_() {
    if (this.floatingPromptResizeRafId_) {
      cancelAnimationFrame(this.floatingPromptResizeRafId_);
      this.floatingPromptResizeRafId_ = 0;
    }
  }

  private updateFloatingPromptPosition(fromResizeObserver: boolean = false) {
    const hideFloatingPrompt = () => {
      if (fromResizeObserver) {
        // Hiding the observed container inside a ResizeObserver callback
        // changes its observed size to 0x0 within the same observation pass and
        // can trigger a ResizeObserver loop error. Defer the update to the next
        // animation frame.
        this.deferFloatingPromptPositionUpdate_();
      } else {
        this.cancelDeferredFloatingPromptPositionUpdate_();
        this.showFloatingPrompt = false;
      }
    };

    if (!this.enableSelectionOverlayPrompt &&
        !this.enableSelectionOverlayPromptBox) {
      hideFloatingPrompt();
      return;
    }

    if (!this.selectionElements.postSelectionRenderer.hasSelection() ||
        this.hasMultipleRegions_()) {
      hideFloatingPrompt();
      return;
    }

    const bounds = this.activeSelection;
    if (!bounds || (bounds.width === 0 && bounds.height === 0)) {
      hideFloatingPrompt();
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

    const extraRowWidth = Math.max(0, this.promptEndpoints.length - 1) *
        (ENDPOINT_CIRCLE_SIZE_PX + ENDPOINT_ROW_GAP_PX);

    const computeLayout = (width: number, height: number) => {
      if (this.enableSelectionOverlayPromptBox) {
        const isRtl = getComputedStyle(this).direction === 'rtl';
        // Sufficient space requires fitting the input box (320px wide, 44px
        // tall) plus any inactive endpoint switcher circle buttons, plus two
        // 1-line contextual suggestions (32px tall each) with 6px gaps
        // (44 + 6 + 32 + 6 + 32 = 120px). The inline fulfillment host prefers
        // 380px width but can shrink down to 320px to fit the available space
        // at the chosen position without moving.
        const minWidth = 320 + extraRowWidth;
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

        // These only place the container. Its content always lines up with
        // the input box's leading edge, even when the container is aligned
        // by its trailing edge.
        const alignStart =
            isRtl ? rightAligned(selRight) : leftAligned(selLeft);
        const alignEnd = isRtl ? leftAligned(selLeft) : rightAligned(selRight);
        const afterSelection = isRtl ? rightAligned(selLeft - margin) :
                                       leftAligned(selRight + margin);
        const beforeSelection = isRtl ? leftAligned(selRight + margin) :
                                        rightAligned(selLeft - margin);
        const insideSelection = isRtl ?
            rightAligned(
                selRight - margin, Math.max(selLeft + margin, margin)) :
            leftAligned(
                selLeft + margin,
                Math.min(selRight - margin, viewportWidth - margin));
        const windowCorner =
            isRtl ? rightAligned(viewportWidth - margin) : leftAligned(margin);

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

        return {
          style: `left: ${chosen.left}px; top: ${chosen.top}px; max-width: ${
              chosen.maxWidth}px;`,
          maxWidth: chosen.maxWidth,
          above: chosen.above,
        };
      }

      // When the prompt box is not shown, anchor the bottom-left of the row at
      // the region's top-right corner (selRight, selTop). When an inline
      // fulfillment card replaces the row, keep the card's top-left corner at
      // the row's top-left corner so it expands downward. The row is positioned
      // by its left edge so the chips grow out of the spark's spot.
      const contentRight = viewportWidth - this.pageScrollbarWidth_;
      const chipsMaxWidth = Math.max(0, contentRight - 2 * margin);
      const chipsWidth = Math.min(width, chipsMaxWidth);
      const clampedLeft = Math.max(
          margin, Math.min(selRight, contentRight - chipsWidth - margin));

      const idealTop = selTop -
          (this.shownInlineAction_ ? this.replacedChipsRowHeight_ : height);
      const targetTop = Math.max(
          margin, Math.min(idealTop, viewportHeight - height - margin));
      return {
        style: `left: ${clampedLeft}px; top: ${targetTop}px; max-width: ${
            chipsMaxWidth}px;`,
        maxWidth: chipsMaxWidth,
        above: false,
      };
    };

    const container =
        this.shadowRoot.querySelector<HTMLElement>('#floatingPromptContainer');
    let promptWidth =
        this.enableSelectionOverlayPromptBox ? 320 + extraRowWidth : 32;
    let promptHeight = this.enableSelectionOverlayPromptBox ? 44 : 32;

    if (container) {
      const rect = container.getBoundingClientRect();
      if (rect.width > 0) {
        promptWidth = rect.width;
        promptHeight = rect.height;
      }
    }

    const layout = computeLayout(promptWidth, promptHeight);
    if (fromResizeObserver &&
        layout.maxWidth !== this.floatingPromptMaxWidth_) {
      // Changing `max-width` on the observed container inside a ResizeObserver
      // callback can resize it within the same observation pass and trigger a
      // ResizeObserver loop error. Defer the update to the next animation
      // frame.
      this.deferFloatingPromptPositionUpdate_();
      return;
    }

    this.cancelDeferredFloatingPromptPositionUpdate_();

    this.promptAboveSelection = layout.above;
    this.floatingPromptMaxWidth_ = layout.maxWidth;
    this.floatingPromptStyle = layout.style;
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
          const refined = computeLayout(newRect.width, newRect.height);
          this.promptAboveSelection = refined.above;
          this.floatingPromptMaxWidth_ = refined.maxWidth;
          this.floatingPromptStyle = refined.style;
        }
      }
    };

    // Refine position once Lit renders the unhidden container and in the next
    // animation frame after layout resolves.
    this.updateComplete.then(refinePosition);
    requestAnimationFrame(refinePosition);
  }

  // With the prompt box and the side panel open, a right drag adds an area and
  // a left drag replaces it.
  protected override dragsOnRightButton(): boolean {
    return this.enableSelectionOverlayPromptBox && this.sidePanelOpen;
  }

  protected override onPointerDown(event: PointerEvent) {
    this.isRightButtonGesture_ = event.button === 2;
    super.onPointerDown(event);
  }

  override handleGestureStart() {
    super.handleGestureStart();
    // A right drag always draws a new area, even over an existing one.
    if (!this.isRightButtonGesture_ &&
        this.selectionElements.postSelectionRenderer.handleGestureStart(
            this.currentGesture)) {
      this.draggingRespondent = DragFeature.POST_SELECTION;
    }
    if (this.draggingRespondent !== DragFeature.NONE ||
        this.canSelectRegion_()) {
      this.showFloatingPrompt = false;
    }
  }

  protected override handleGestureDrag(event: PointerEvent) {
    assert(this.currentGesture.state === GestureState.DRAGGING);
    // Capture pointer events so gestures still work if the users pointer
    // leaves the selection overlay div. Pointer capture is implicitly
    // released after pointerup or pointercancel events.
    this.setPointerCapture(event.pointerId);

    if (this.draggingRespondent === DragFeature.POST_SELECTION) {
      this.showFloatingPrompt = false;
      this.selectionElements.postSelectionRenderer.handleGestureDrag(
          this.currentGesture);
      return;
    }

    // If no one is responding to the drag yet, then let the region selection
    // layer respond.
    if (this.draggingRespondent === DragFeature.NONE) {
      // TODO(b/565871325): Allow adding regions while Ctrl or Cmd is held, here
      // and in `handleGestureEnd()`.
      if (!this.canSelectRegion_()) {
        return;
      }
      this.setCursorToCrosshair();
      this.draggingRespondent = DragFeature.MANUAL_REGION;
      this.hideHandles = false;

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
      this.showFloatingPrompt = false;
      this.selectionElements.regionSelectionLayer.handleGestureDrag(
          this.currentGesture);
    }
  }

  protected override handleGestureEnd() {
    if (!this.canSelectRegion_() &&
        this.draggingRespondent === DragFeature.NONE) {
      if (!this.enableSelectionOverlayPromptBox) {
        (this.baseHandler as SelectionOverlayBaseHandlerImpl)
            .dismissOverlay(DismissOverlayReason.kBackgroundClick);
      }
      return;
    }

    // Allow proper feature to respond to the tap/drag event.
    switch (this.currentGesture.state) {
      case GestureState.DRAGGING:

        // Drag has finished. Let the features respond to the end of a drag.
        if (this.draggingRespondent === DragFeature.MANUAL_REGION) {
          this.selectionElements.regionSelectionLayer.handleGestureEnd(
              this.currentGesture);
          this.fetchSuggestedActions();
        } else if (this.draggingRespondent === DragFeature.POST_SELECTION) {
          this.selectionElements.postSelectionRenderer.handleGestureEnd();
          // Fade out scrim which is currently being managed by region selection
          // TODO(crbug.com/420998632): move scrim out to its own component
          this.selectionElements.regionSelectionLayer
              .handlePostSelectionDragGestureEnd();
          this.fetchSuggestedActions();
        }
        break;
      case GestureState.STARTING:
        if (this.draggingRespondent === DragFeature.NONE) {
          // With the prompt box, a click doesn't select an area around it or
          // dismiss the overlay.
          if (this.enableSelectionOverlayPromptBox) {
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

  protected hasMultipleEndpoints(): boolean {
    return this.promptEndpoints.length > 1;
  }

  private getActiveEndpoint_(): PromptEndpointInfo|undefined {
    return this.promptEndpoints[this.activeEndpointIndex] ??
        this.promptEndpoints[0];
  }

  protected getActiveEndpointPlaceholder(): string {
    return this.getActiveEndpoint_()?.placeholder ??
        loadTimeData.getString('askGemini');
  }

  protected getEndpointIconClass(id?: PromptEndpointId): string {
    switch (id) {
      case PromptEndpointId.kGoogleSearch:
        return 'google-search';
      case PromptEndpointId.kGemini:
      default:
        return 'gemini';
    }
  }

  protected getActiveEndpointIconClass(): string {
    return this.getEndpointIconClass(this.getActiveEndpoint_()?.id);
  }

  protected getInactiveEndpoints():
      Array<{endpoint: PromptEndpointInfo, index: number}> {
    if (!this.hasMultipleEndpoints()) {
      return [];
    }
    const count = this.promptEndpoints.length;
    const active =
        this.activeEndpointIndex >= 0 && this.activeEndpointIndex < count ?
        this.activeEndpointIndex :
        0;
    const result: Array<{endpoint: PromptEndpointInfo, index: number}> = [];
    for (let offset = 1; offset < count; offset++) {
      const index = (active + offset) % count;
      const endpoint = this.promptEndpoints[index];
      if (endpoint) {
        result.push({endpoint, index});
      }
    }
    return result;
  }

  protected getEndpointSwitchTitle(): string {
    return this.hasMultipleEndpoints() ?
        loadTimeData.getString('switchProviderTooltip') :
        '';
  }

  private finishSwitchAnimation_() {
    this.switchAnimationToken_++;
    if (this.switchCleanupTimer_) {
      clearTimeout(this.switchCleanupTimer_);
      this.switchCleanupTimer_ = 0;
    }
    const animations = this.switchAnimations_;
    this.switchAnimations_ = [];
    for (const animation of animations) {
      animation.onfinish = null;
      animation.oncancel = null;
      animation.cancel();
    }
    const cleanup = this.switchCleanup_;
    this.switchCleanup_ = null;
    cleanup?.();
  }

  private switchToEndpoint_(newIndex: number) {
    if (!this.hasMultipleEndpoints()) {
      return;
    }
    const count = this.promptEndpoints.length;
    if (!Number.isInteger(newIndex) || newIndex < 0 || newIndex >= count ||
        newIndex === this.activeEndpointIndex) {
      return;
    }

    this.finishSwitchAnimation_();

    const prefersReducedMotion =
        window.matchMedia('(prefers-reduced-motion: reduce)').matches;
    const searchboxRow =
        this.shadowRoot.querySelector<HTMLElement>('.searchbox-row');
    const pill = this.shadowRoot.querySelector<HTMLElement>('.searchbox-pill');
    let ghost: HTMLElement|null = null;
    let rowRect: DOMRect|null = null;
    let capRect: DOMRect|null = null;
    let sourceRect: DOMRect|null = null;
    const oldRects = new Map<number, DOMRect>();

    if (this.showFloatingPrompt && !prefersReducedMotion && searchboxRow &&
        pill) {
      rowRect = searchboxRow.getBoundingClientRect();
      capRect = pill.getBoundingClientRect();
      if (capRect.width > 0) {
        this.shadowRoot
            .querySelectorAll<HTMLElement>('.endpoint-switcher-button')
            .forEach(button => {
              const idx = Number(button.dataset['endpointIndex']);
              oldRects.set(idx, button.getBoundingClientRect());
            });
        sourceRect = oldRects.get(newIndex) ?? capRect;
        ghost = createPillGhost(pill);
      }
    }

    this.activeEndpointIndex = newIndex;
    const activeEndpoint = this.getActiveEndpoint_();
    if (!activeEndpoint) {
      return;
    }
    this.endpointLiveAnnouncement = activeEndpoint.name;
    (this.baseHandler as SelectionOverlayBaseHandlerImpl)
        .setActivePromptEndpoint(activeEndpoint.id);
    this.fetchSuggestedActions();

    if (!ghost || !rowRect || !capRect || !sourceRect || !searchboxRow ||
        !pill) {
      return;
    }

    void this.animateEndpointSwitch_({
      searchboxRow,
      pill,
      ghost,
      rowRect,
      capRect,
      sourceRect,
      oldRects,
    });
  }

  private async animateEndpointSwitch_(params: {
    searchboxRow: HTMLElement,
    pill: HTMLElement,
    ghost: HTMLElement,
    rowRect: DOMRect,
    capRect: DOMRect,
    sourceRect: DOMRect,
    oldRects: Map<number, DOMRect>,
  }) {
    const {searchboxRow, pill, ghost, rowRect, capRect, sourceRect, oldRects} =
        params;
    const token = ++this.switchAnimationToken_;
    await this.updateComplete;
    if (token !== this.switchAnimationToken_ || !this.isConnected ||
        !this.showFloatingPrompt || !searchboxRow.isConnected ||
        !pill.isConnected) {
      return;
    }

    const isRtl = getComputedStyle(this).direction === 'rtl';
    const capTop = capRect.top - rowRect.top;
    const capWidth = capRect.width;
    const capHeight = capRect.height;

    ghost.style.top = `${capTop}px`;
    ghost.style.width = `${capWidth}px`;
    ghost.style.height = `${capHeight}px`;
    ghost.style.minWidth = '0px';
    if (isRtl) {
      ghost.style.right = `${rowRect.right - capRect.right}px`;
    } else {
      ghost.style.left = `${capRect.left - rowRect.left}px`;
    }
    searchboxRow.appendChild(ghost);

    // Hold the pill's flex slot with a fixed spacer while the pill expands
    // out of flow, so circle buttons and the container size never shift.
    const spacer = document.createElement('div');
    spacer.className = 'searchbox-pill-spacer';
    spacer.style.width = `${capWidth}px`;
    spacer.style.height = `${capHeight}px`;
    pill.after(spacer);

    pill.style.position = 'absolute';
    pill.style.top = `${capTop}px`;
    pill.style.width = `${capWidth}px`;
    if (isRtl) {
      pill.style.right = `${rowRect.right - capRect.right}px`;
    } else {
      pill.style.left = `${capRect.left - rowRect.left}px`;
    }

    const animations: Animation[] = [];

    // Outgoing pill collapses toward the start edge and fades out.
    const ghostAnim = ghost.animate(
        [
          {
            width: `${capWidth}px`,
            opacity: 1,
            transform: 'translateX(0)',
          },
          {
            width: `${ENDPOINT_CIRCLE_SIZE_PX}px`,
            opacity: 0,
            transform: `translateX(${isRtl ? 10 : -10}px)`,
          },
        ],
        {
          duration: Math.round(ENDPOINT_SWITCH_DURATION_MS * 0.85),
          easing: ENDPOINT_SWITCH_EASING,
          fill: 'forwards',
        });
    ghostAnim.onfinish = () => ghost.remove();
    ghostAnim.oncancel = () => ghost.remove();
    animations.push(ghostAnim);

    // Incoming pill expands from the chosen provider's circle button.
    const dx = isRtl ? sourceRect.right - capRect.right :
                       sourceRect.left - capRect.left;
    const startW = sourceRect.width || ENDPOINT_CIRCLE_SIZE_PX;
    const capAnim = pill.animate(
        [
          {
            transform: `translateX(${dx}px)`,
            width: `${startW}px`,
            minWidth: `${startW}px`,
          },
          {
            transform: 'translateX(0)',
            width: `${capWidth}px`,
            minWidth: `${capWidth}px`,
          },
        ],
        {
          duration: ENDPOINT_SWITCH_DURATION_MS,
          easing: ENDPOINT_SWITCH_EASING,
        });
    animations.push(capAnim);

    // Input contents fade in as the pill expands.
    for (const child of pill.children) {
      if (!(child instanceof HTMLElement) || child.id === 'endpointIcon' ||
          child.id === 'endpointLiveRegion' || child.hidden ||
          getComputedStyle(child).display === 'none') {
        continue;
      }
      animations.push(child.animate(
          [
            {opacity: 0},
            {opacity: 0, offset: 0.35},
            {opacity: 1},
          ],
          {duration: ENDPOINT_SWITCH_DURATION_MS, easing: 'ease-out'}));
    }

    // Circle buttons moving toward the start edge FLIP-slide; newly arrived
    // buttons (including the previously active provider) slide in from the
    // far end while fading in.
    let enterDelay = 70;
    const enterOffset =
        isRtl ? -ENDPOINT_CIRCLE_SIZE_PX : ENDPOINT_CIRCLE_SIZE_PX;
    this.shadowRoot.querySelectorAll<HTMLElement>('.endpoint-switcher-button')
        .forEach(button => {
          const idx = Number(button.dataset['endpointIndex']);
          const before = oldRects.get(idx);
          const after = button.getBoundingClientRect();
          const movedTowardStart = before &&
              (isRtl ? before.left < after.left - 0.5 :
                       before.left > after.left + 0.5);
          if (movedTowardStart) {
            animations.push(button.animate(
                [
                  {transform: `translateX(${before.left - after.left}px)`},
                  {transform: 'translateX(0)'},
                ],
                {
                  duration: ENDPOINT_SWITCH_DURATION_MS,
                  easing: ENDPOINT_SWITCH_EASING,
                }));
          } else {
            animations.push(button.animate(
                [
                  {opacity: 0, transform: `translateX(${enterOffset}px)`},
                  {opacity: 1, transform: 'translateX(0)'},
                ],
                {
                  duration: ENDPOINT_SWITCH_DURATION_MS,
                  easing: ENDPOINT_SWITCH_EASING,
                  delay: enterDelay,
                  fill: 'backwards',
                }));
            enterDelay += 60;
          }
        });

    this.switchAnimations_ = animations;

    let cleanedUp = false;
    const cleanup = () => {
      if (cleanedUp) {
        return;
      }
      cleanedUp = true;
      if (this.switchCleanupTimer_) {
        clearTimeout(this.switchCleanupTimer_);
        this.switchCleanupTimer_ = 0;
      }
      if (this.switchCleanup_ === cleanup) {
        this.switchCleanup_ = null;
      }
      spacer.remove();
      pill.style.position = '';
      pill.style.left = '';
      pill.style.right = '';
      pill.style.top = '';
      pill.style.width = '';
      ghost.remove();
    };

    this.switchCleanup_ = cleanup;
    capAnim.onfinish = cleanup;
    capAnim.oncancel = cleanup;
    this.switchCleanupTimer_ =
        setTimeout(cleanup, ENDPOINT_SWITCH_DURATION_MS + 200);
  }

  private cycleEndpoint_(delta: number) {
    if (!this.hasMultipleEndpoints()) {
      return;
    }
    const count = this.promptEndpoints.length;
    this.switchToEndpoint_((this.activeEndpointIndex + delta + count) % count);
  }

  protected onEndpointSwitcherMousedown(event: MouseEvent): void {
    // Keep focus in the prompt input when clicking a provider circle button.
    event.preventDefault();
  }

  protected onEndpointSwitcherClick(event: Event): void {
    event.stopPropagation();
    const index =
        Number((event.currentTarget as HTMLElement).dataset['endpointIndex']);
    this.switchToEndpoint_(index);
    this.focusPromptInput_();
  }

  protected onEndpointSwitcherKeydown(event: KeyboardEvent): void {
    if (event.key === 'Escape') {
      (this.baseHandler as SelectionOverlayBaseHandlerImpl)
          .dismissOverlay(DismissOverlayReason.kCloseButton);
    }
  }

  private hasModifierKey_(event: KeyboardEvent, allowShift: boolean = false):
      boolean {
    return (!allowShift && event.shiftKey) || event.ctrlKey || event.altKey ||
        event.metaKey;
  }

  protected onInputKeydown(event: KeyboardEvent) {
    if (event.isComposing) {
      return;
    }
    if (event.key === 'Tab' &&
        !this.hasModifierKey_(event, /*allowShift=*/ true) &&
        this.hasMultipleEndpoints()) {
      event.preventDefault();
      this.cycleEndpoint_(event.shiftKey ? -1 : 1);
      return;
    }
    if (event.key === 'Enter') {
      this.submitPromptInput_();
      return;
    }
    if (event.key === 'Escape') {
      (this.baseHandler as SelectionOverlayBaseHandlerImpl)
          .dismissOverlay(DismissOverlayReason.kCloseButton);
      return;
    }
    const moveIntoChipsKey =
        this.promptAboveSelection ? 'ArrowUp' : 'ArrowDown';
    if (event.key === moveIntoChipsKey && !this.hasModifierKey_(event)) {
      const firstChip =
          this.shadowRoot.querySelector<HTMLElement>('.action-chip');
      if (firstChip) {
        event.preventDefault();
        firstChip.focus();
      }
    }
  }

  protected onActionChipKeydown(event: KeyboardEvent) {
    if (event.key === 'Escape') {
      (this.baseHandler as SelectionOverlayBaseHandlerImpl)
          .dismissOverlay(DismissOverlayReason.kCloseButton);
      return;
    }
    if (!this.enableSelectionOverlayPromptBox || this.hasModifierKey_(event)) {
      return;
    }
    const chips = Array.from(
        this.shadowRoot.querySelectorAll<HTMLElement>('.action-chip'));
    const index = Number((event.currentTarget as HTMLElement).dataset['index']);
    const awayFromInputKey =
        this.promptAboveSelection ? 'ArrowUp' : 'ArrowDown';
    const towardInputKey = this.promptAboveSelection ? 'ArrowDown' : 'ArrowUp';
    if (event.key === awayFromInputKey) {
      if (index + 1 < chips.length) {
        event.preventDefault();
        chips[index + 1]!.focus();
      }
    } else if (event.key === towardInputKey) {
      event.preventDefault();
      if (index > 0) {
        chips[index - 1]!.focus();
      } else {
        this.focusPromptInput_();
      }
    }
  }

  protected onSubmitPromptClick(event: Event) {
    event.stopPropagation();
    this.submitPromptInput_();
  }

  protected onSearchboxPillClick(): void {
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

  private submitPrompt(prompt: string) {
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
