// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/lens/region_selection.js';
import '/lens/post_selection_renderer.js';
import '/lens/overlay_border_glow.js';
import '/lens/overlay_shimmer_canvas.js';
import '/strings.m.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {OverlayBorderGlowElement} from '/lens/overlay_border_glow.js';
import type {OverlayShimmerCanvasElement} from '/lens/overlay_shimmer_canvas.js';
import type {PostSelectionBoundingBox, PostSelectionRendererElement} from '/lens/post_selection_renderer.js';
import type {RegionSelectionElement} from '/lens/region_selection.js';
import {SelectionOverlayBaseLitElement} from '/lens/selection_overlay_base_lit.js';
import {DragFeature, GestureState} from '/lens/selection_utils.js';

import {getCss} from './glic_selection_overlay.css.js';
import {getHtml} from './glic_selection_overlay.html.js';
import {DismissOverlayReason} from './selection_overlay.mojom-webui.js';
import type {SuggestedAction} from './selection_overlay.mojom-webui.js';
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
export interface SelectionOverlayElementElement {
  $: {
    backgroundImageCanvas: HTMLCanvasElement,
    cursor: HTMLElement,
    initialFlashScrim: HTMLElement,
    overlayShimmerCanvas: OverlayShimmerCanvasElement,
    postSelectionRenderer: PostSelectionRendererElement,
    regionSelectionLayer: RegionSelectionElement,
    selectionOverlay: HTMLElement,
    floatingPromptContainer?: HTMLElement,
    promptInput?: HTMLInputElement,
  };
}

export class SelectionOverlayElementElement extends
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
      floatingPromptStyle: {type: String},
      activeSelection: {type: Object},
      suggestedActions: {type: Array},
      enableSelectionOverlayPrompt: {type: Boolean},
    };
  }

  accessor screenshotDataUri: string = '';
  accessor enableRegionSelectedGlow: boolean = true;
  override accessor enableBorderGlow: boolean = true;
  override accessor disableShimmer: boolean = false;
  accessor showFloatingPrompt: boolean = false;
  accessor floatingPromptStyle: string = '';
  accessor activeSelection: PostSelectionBoundingBox|null = null;
  accessor suggestedActions: SuggestedAction[] = [];
  accessor enableSelectionOverlayPrompt: boolean =
      loadTimeData.getBoolean('enableSelectionOverlayPrompt');

  constructor() {
    super();
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

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
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
          this.activeSelection = e.detail;
          if (this.currentGesture?.state === GestureState.NOT_STARTED ||
              this.currentGesture?.state === undefined) {
            this.updateFloatingPromptPosition();
          }
        });

    this.eventTracker_.add(document, 'post-selection-cleared', () => {
      this.activeSelection = null;
      this.showFloatingPrompt = false;
    });

    if (this.enableSelectionOverlayPrompt) {
      const handlerImpl = this.baseHandler as SelectionOverlayBaseHandlerImpl;
      handlerImpl.getSuggestedActions().then((actions: SuggestedAction[]) => {
        this.suggestedActions = actions;
        if (this.showFloatingPrompt) {
          this.updateFloatingPromptPosition();
        }
      });
    }

    this.eventTracker_.add(window, 'resize', () => {
      if (this.showFloatingPrompt) {
        this.updateFloatingPromptPosition();
      }
    });
  }

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

  protected override get defaultCursorIconUrl() {
    return 'url("/glic_region_selection_cursor_icon.svg")';
  }

  private updateFloatingPromptPosition() {
    if (!this.enableSelectionOverlayPrompt) {
      this.showFloatingPrompt = false;
      return;
    }

    if (!this.selectionElements.postSelectionRenderer.hasSelection()) {
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
    const selCenterX = selLeft + selWidth / 2;

    const margin = 16;
    const viewportWidth = window.innerWidth;
    const viewportHeight = window.innerHeight;

    const container =
        this.shadowRoot.querySelector<HTMLElement>('#floatingPromptContainer');
    let promptWidth = 360;
    let promptHeight = 96;

    if (container) {
      const rect = container.getBoundingClientRect();
      if (rect.width > 0) {
        promptWidth = rect.width;
        promptHeight = rect.height;
      }
    }

    const maxAvailableWidth = Math.max(0, viewportWidth - 2 * margin);
    const effectiveWidth = Math.min(promptWidth, maxAvailableWidth);

    let clampedCenterX: number;
    if (effectiveWidth >= maxAvailableWidth) {
      clampedCenterX = viewportWidth / 2;
    } else {
      const minCenter = effectiveWidth / 2 + margin;
      const maxCenter = viewportWidth - effectiveWidth / 2 - margin;
      clampedCenterX = Math.max(minCenter, Math.min(selCenterX, maxCenter));
    }

    let targetTop = selBottom + margin;
    if (targetTop + promptHeight > viewportHeight - margin) {
      const topAbove = selTop - promptHeight - margin;
      if (topAbove >= margin) {
        targetTop = topAbove;
      } else {
        targetTop = Math.max(margin, viewportHeight - promptHeight - margin);
      }
    }

    this.floatingPromptStyle = `left: ${clampedCenterX}px; top: ${
        targetTop}px; transform: translateX(-50%);`;
    this.showFloatingPrompt = true;

    // Refine position in the next animation frame after layout resolves to
    // ensure any dynamic chip widths or wrapping are accounted for.
    requestAnimationFrame(() => {
      if (!this.showFloatingPrompt) {
        return;
      }
      const currentContainer = this.shadowRoot.querySelector<HTMLElement>(
          '#floatingPromptContainer');
      if (currentContainer) {
        const newRect = currentContainer.getBoundingClientRect();
        if (Math.abs(newRect.width - promptWidth) > 2 ||
            Math.abs(newRect.height - promptHeight) > 2) {
          const newEffectiveWidth = Math.min(newRect.width, maxAvailableWidth);
          let newCenterX: number;
          if (newEffectiveWidth >= maxAvailableWidth) {
            newCenterX = viewportWidth / 2;
          } else {
            const minC = newEffectiveWidth / 2 + margin;
            const maxC = viewportWidth - newEffectiveWidth / 2 - margin;
            newCenterX = Math.max(minC, Math.min(selCenterX, maxC));
          }
          let newTop = selBottom + margin;
          if (newTop + newRect.height > viewportHeight - margin) {
            const above = selTop - newRect.height - margin;
            if (above >= margin) {
              newTop = above;
            } else {
              newTop =
                  Math.max(margin, viewportHeight - newRect.height - margin);
            }
          }
          this.floatingPromptStyle = `left: ${newCenterX}px; top: ${
              newTop}px; transform: translateX(-50%);`;
        }
      }
    });
  }

  override handleGestureStart() {
    super.handleGestureStart();
    this.showFloatingPrompt = false;
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
      this.setCursorToCrosshair();
      this.draggingRespondent = DragFeature.MANUAL_REGION;

      this.activeRegionId = '';
      this.baseHandler.activeRegionId = '';
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
        break;
      case GestureState.STARTING:
        if (this.draggingRespondent === DragFeature.NONE) {
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

  protected onInputKeydown(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      const input =
          this.shadowRoot.querySelector<HTMLInputElement>('#promptInput');
      if (input && input.value.trim().length > 0) {
        this.submitPrompt(input.value.trim());
      }
    } else if (event.key === 'Escape') {
      (this.baseHandler as SelectionOverlayBaseHandlerImpl)
          .dismissOverlay(DismissOverlayReason.kCloseButton);
    }
  }

  protected onSuggestedActionClick(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const action = this.suggestedActions[index];
    if (action) {
      (this.baseHandler as SelectionOverlayBaseHandlerImpl)
          .executeSuggestedAction(action.id);
    }
  }

  protected submitPrompt(prompt: string) {
    (this.baseHandler as SelectionOverlayBaseHandlerImpl).submitPrompt(prompt);
  }

  protected onPromptPointerdown(event: PointerEvent) {
    event.stopPropagation();
  }

  protected getActionIcon(title: string) {
    switch (title.toLowerCase()) {
      case 'explain':
        return html`
          <svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor">
            <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-6h2v6zm0-8h-2V7h2v2z"/>
          </svg>`;
      case 'summarize':
        return html`
          <svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor">
            <path d="M14 17H4v-2h10v2zm6-8H4V7h16v2zm0 4H4v-2h16v2zm-6 8H4v-2h10v2z"/>
          </svg>`;
      case 'create image':
        return html`
          <svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor">
            <path d="M21 19V5c0-1.1-.9-2-2-2H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2zM8.5 13.5l2.5 3.01L14.5 12l4.5 6H5l3.5-4.5z"/>
          </svg>`;
      default:
        return html`
          <svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor">
            <path d="M12 2C12 7.52 7.52 12 2 12C7.52 12 12 16.48 12 22C12 16.48 16.48 12 22 12C16.48 12 12 7.52 12 2Z"/>
          </svg>`;
    }
  }

  override getOverlayBorderGlow(): OverlayBorderGlowElement {
    return this.shadowRoot.querySelector<OverlayBorderGlowElement>(
        '#overlayBorderGlow')!;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'glic-selection-overlay': SelectionOverlayElementElement;
  }
}

customElements.define(
    SelectionOverlayElementElement.is, SelectionOverlayElementElement);
