// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './scrollbar.html.js';

export interface HealthdInternalsLineChartScrollbarElement {
  $: {
    outerDiv: HTMLElement,
    innerDiv: HTMLElement,
  };
}

/**
 * Create two div blocks for displaying scrollbar, which is used to show the
 * position of line chart and to scroll the line chart.
 *
 * The width of outer div will be the same as visible chart width. And the width
 * of inner div will be the same as the width of whole chart.
 *
 * With the scrollbar, we can draw the visible part of line chart only instead
 * of the whole chart .
 */
export class HealthdInternalsLineChartScrollbarElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-line-chart-scrollbar';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.$.outerDiv.addEventListener('scroll', () => this.onScroll());
  }

  // The range the scrollbar can scroll.
  private scrollableRange: number = 0;
  // The current position of the scrollbar.
  private currentPosition: number = 0;
  // The visible width of this scrollbar.
  private visibleWidth: number = 0;

  // Scrolling event handler.
  private onScroll() {
    const newPosition: number = this.$.outerDiv.scrollLeft;
    if (newPosition === this.currentPosition) {
      return;
    }
    this.currentPosition = newPosition;
    this.dispatchEvent(
        new CustomEvent('bar-scroll', {bubbles: true, composed: true}));
  }

  // Return the height of scrollbar element.
  getHeight(): number {
    return this.$.outerDiv.offsetHeight;
  }

  getScrollableRange(): number {
    return this.scrollableRange;
  }

  // Position may be float point number because `scrollLeft` may be float point
  // number.
  getPosition(): number {
    return Math.round(this.currentPosition);
  }

  // Change the size of the outer div and update the scrollbar position.
  resize(width: number) {
    if (this.visibleWidth === width) {
      return;
    }
    this.visibleWidth = width;
    this.$.outerDiv.style.width = this.visibleWidth + 'px';
  }

  // Set the scrollable range to `range`. Use the inner div's width to control
  // the scrollable range. If position go out of range after range update, set
  // it to the boundary value.
  setScrollableRange(range: number) {
    this.scrollableRange = range;
    this.$.innerDiv.style.width =
        (this.visibleWidth + this.scrollableRange) + 'px';
    if (range < this.currentPosition) {
      this.currentPosition = range;
      this.updateScrollbarPosition();
    }
  }

  // Set the scrollbar position to `position`. If the new position go out of
  // range, set it to the boundary value.
  setPosition(position: number) {
    const newPosition: number =
        Math.max(0, Math.min(position, this.scrollableRange));
    this.currentPosition = newPosition;
    this.updateScrollbarPosition();
  }

  // Return true if scrollbar is at the right edge of the chart.
  isScrolledToRightEdge(): boolean {
    // `scrollLeft` may become a float point number even if we set it to some
    // integer value. If the distance to the right edge less than 2 pixels, we
    // consider that it is scrolled to the right edge.
    const scrollLeftErrorAmount: number = 2;
    return this.currentPosition + scrollLeftErrorAmount > this.scrollableRange;
  }

  // Scroll the scrollbar to the right edge.
  scrollToRightEdge() {
    this.setPosition(this.scrollableRange);
  }

  // Update the scrollbar position.
  private updateScrollbarPosition() {
    if (this.$.outerDiv.scrollLeft === this.currentPosition) {
      return;
    }
    this.$.outerDiv.scrollLeft = this.currentPosition;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-line-chart-scrollbar':
        HealthdInternalsLineChartScrollbarElement;
  }
}

customElements.define(
    HealthdInternalsLineChartScrollbarElement.is,
    HealthdInternalsLineChartScrollbarElement);
