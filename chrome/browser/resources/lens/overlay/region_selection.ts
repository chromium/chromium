// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './region_selection.html.js';
import type {GestureEvent} from './selection_utils.js';

export interface RegionSelectionElement {
  $: {regionSelectionCanvas: HTMLCanvasElement};
}

/*
 * Element responsible for rendering the region being selected by the user. It
 * does not render any post-selection state.
 */
export class RegionSelectionElement extends PolymerElement {
  static get is() {
    return 'region-selection';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      canvasHeight: Number,
      canvasWidth: Number,
    };
  }

  private canvasHeight: number;
  private canvasWidth: number;
  private context: CanvasRenderingContext2D;
  private parentResizeObserver: ResizeObserver = new ResizeObserver(() => {
    this.setCanvasSizeToParent();
  });

  override connectedCallback() {
    super.connectedCallback();

    assert(this.parentElement);
    this.parentResizeObserver.observe(this.parentElement);
  }

  override disconnectedCallback() {
    assert(this.parentElement);
    this.parentResizeObserver.unobserve(this.parentElement);
  }

  override ready() {
    super.ready();

    this.context = this.$.regionSelectionCanvas.getContext('2d')!;
  }

  // Handles a drag gesture by drawing a bounded box on the canvas.
  handleDragGesture(event: GestureEvent) {
    this.clearCanvas();
    this.renderDashedBoundingBox(event);
  }

  private setCanvasSizeToParent() {
    assert(this.parentElement);
    // Resetting the canvas width and height also clears the canvas.
    this.canvasWidth = this.parentElement.offsetWidth;
    this.canvasHeight = this.parentElement.offsetHeight;
  }

  private clearCanvas() {
    this.context.clearRect(0, 0, this.canvasWidth, this.canvasHeight);
  }

  private renderDashedBoundingBox(event: GestureEvent, idealCornerRadius = 12) {
    const dashLength = 6;
    const gapLength = 5;

    // Get the dimensions of the box from the gesture event points.
    const width = Math.abs(event.clientX - event.startX);
    const height = Math.abs(event.clientY - event.startY);

    // Find the offsets of the canvas relative to the window.
    const rect = this.$.regionSelectionCanvas.getBoundingClientRect();
    const offsetX = rect.left;
    const offsetY = rect.top;

    // Define the points for the bounding box for readability.
    const topLeftX = Math.min(event.clientX - offsetX, event.startX - offsetX);
    const topLeftY = Math.min(event.clientY - offsetY, event.startY - offsetY);

    this.context.setLineDash([dashLength, gapLength]);
    this.context.lineWidth = 2;
    this.context.fillStyle = 'rgba(255, 255, 255, 0.3)';
    this.context.strokeStyle = 'white';

    // Draw the path for the region bounding box.
    this.context.beginPath();
    this.context.roundRect(
        topLeftX, topLeftY, width, height, idealCornerRadius);
    this.context.stroke();
    this.context.fill();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'region-selection': RegionSelectionElement;
  }
}

customElements.define(RegionSelectionElement.is, RegionSelectionElement);
