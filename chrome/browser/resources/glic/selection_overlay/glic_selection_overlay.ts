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
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {getFallbackTheme} from '/lens/color_utils.js';
import type {OverlayBorderGlowElement} from '/lens/overlay_border_glow.js';
import type {OverlayShimmerCanvasElement} from '/lens/overlay_shimmer_canvas.js';
import type {PostSelectionRendererElement} from '/lens/post_selection_renderer.js';
import type {RegionSelectionElement} from '/lens/region_selection.js';
import {SelectionOverlayBaseLitElement} from '/lens/selection_overlay_base_lit.js';
import {DragFeature, GestureState} from '/lens/selection_utils.js';

import {getCss} from './glic_selection_overlay.css.js';
import {getHtml} from './glic_selection_overlay.html.js';

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
    };
  }

  accessor screenshotDataUri: string = '';
  accessor enableRegionSelectedGlow: boolean = true;
  override accessor enableBorderGlow: boolean = true;
  override accessor disableShimmer: boolean = false;

  constructor() {
    super();
    this.theme = getFallbackTheme();
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

    if (changedProperties.has('theme' as any)) {
      this.updateThemeColors();
    }
  }

  override firstUpdated() {
    super.firstUpdated();
    GLIC_BORDER_GLOW_COLORS.forEach((color, index) => {
      this.style.setProperty(`--overlay-border-glow-color-${index + 1}`, color);
    });
    this.updateThemeColors();
    this.resetCursor();
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

  override handleGestureStart() {
    super.handleGestureStart();
    if (this.selectionElements.postSelectionRenderer.handleGestureStart(
            this.currentGesture)) {
      this.draggingRespondent = DragFeature.POST_SELECTION;
    }
  }

  protected override handleGestureDrag(event: PointerEvent) {
    assert(this.currentGesture.state === GestureState.DRAGGING);
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
          this.selectionElements.regionSelectionLayer.handleGestureEnd(
              this.currentGesture);
        }
        break;
      default:  // Other states are invalid and ignored.
        break;
    }

    this.resetCursor();
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
