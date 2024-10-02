// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './horizontal_carousel.css.js';
import {getHtml} from './horizontal_carousel.html.js';
import {$$} from './utils.js';

/**
 * @fileoverview
 * Navigates a `product-specifications-table` horizontally by one column width,
 * using forward and backward buttons.
 *
 * Prevents vertical scrolling by directly controlling horizontal scroll
 * position. Updates button visibility using `IntersectionObserver` to detect if
 * either end of the container has been reached.
 *
 * Relies on CSS scroll-snap properties for precise column alignment:
 *  - `scroll-snap-type: x mandatory` on container
 *  - `scroll-snap-align: start` on `product-specifications-table`'s columns
 *
 * Note: Key differences between this HorizontalCarouselElement and the one
 * located at ui/webui/resources/cr_components/history_clusters.
 *    - This carousel considers the width of its slotted children, and uses
 * CSS scroll-snap properties to enable fine-grained control over how much
 * content is shown/hidden at once.
 *    - The other carousel bases its scrolling on the container's width, making
 * it less suitable for cases where precise content control is needed.
 */

export interface HorizontalCarouselElement {
  $: {
    backButton: HTMLElement,
    backButtonContainer: HTMLElement,
    carouselContainer: HTMLElement,
    endProbe: HTMLElement,
    forwardButton: HTMLElement,
    forwardButtonContainer: HTMLElement,
    slottedTable: HTMLSlotElement,
    startProbe: HTMLElement,
  };
}

export class HorizontalCarouselElement extends CrLitElement {
  static get is() {
    return 'horizontal-carousel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * True if slotted table is overflown, regardless of any elements that may
       * appear before or after it.
       */
      canScroll_: {
        type: Boolean,
        reflect: true,
      },

      /**
         True if slotted table is overflown on the left side of the carousel.
       */
      showBackButton_: {type: Boolean},

      /**
         True if slotted table is overflown on the right side of the carousel.
       */
      showForwardButton: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  showForwardButton: boolean = false;
  protected canScroll_: boolean = false;
  protected showBackButton_: boolean = false;

  private intersectionObserver_: IntersectionObserver|null = null;
  private resizeObserver_: ResizeObserver|null = null;
  private scrolledToEnd_: boolean = false;
  private scrolledToStart_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.intersectionObserver_ = this.createIntersectionObserver_();
    this.resizeObserver_ = this.createResizeObserver_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.intersectionObserver_) {
      this.intersectionObserver_.disconnect();
      this.intersectionObserver_ = null;
    }
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
      this.resizeObserver_ = null;
    }
  }

  protected onCarouselBackClick_() {
    this.$.carouselContainer.scrollBy({left: -1 * this.columnOffsetWidth_});
  }

  protected onCarouselForwardClick_() {
    this.$.carouselContainer.scrollBy({left: this.columnOffsetWidth_});
  }

  private createIntersectionObserver_(): IntersectionObserver {
    const observer = new IntersectionObserver(entries => {
      let tmpCanScroll = false;
      entries.forEach(entry => {
        const {target, intersectionRatio} = entry;
        if (target === this.$.startProbe) {
          tmpCanScroll = intersectionRatio === 0 || !this.scrolledToEnd_;
          this.scrolledToStart_ = intersectionRatio !== 0;
        } else if (target === this.$.endProbe) {
          tmpCanScroll = intersectionRatio === 0 || !this.scrolledToStart_;
          this.scrolledToEnd_ = intersectionRatio !== 0;
        }
      });
      this.canScroll_ = tmpCanScroll;
      this.showBackButton_ = tmpCanScroll && !this.scrolledToStart_;
      this.showForwardButton = tmpCanScroll && !this.scrolledToEnd_;
      this.dispatchEvent(new CustomEvent(
          'intersection-observed', {bubbles: true, composed: true}));
    }, {root: this.$.carouselContainer});
    observer.observe(this.$.startProbe);
    observer.observe(this.$.endProbe);
    return observer;
  }

  private createResizeObserver_(): ResizeObserver {
    const observer = new ResizeObserver(() => {
      const carouselHeight =
          this.$.carouselContainer.getBoundingClientRect().height;
      const carouselOffset =
          this.$.carouselContainer.getBoundingClientRect().top;
      if (carouselHeight > window.innerHeight - carouselOffset) {
        // Reset to CSS defined style.
        this.$.backButtonContainer.attributeStyleMap.delete('top');
        this.$.forwardButtonContainer.attributeStyleMap.delete('top');
        return;
      }

      // Force the carousel arrows to appear in the middle of the table, since
      // the percentage value uses the nearest scrolling ancestor's height.
      const buttonTopPx = carouselOffset + carouselHeight * 0.44;
      this.$.backButtonContainer.style.top = `${buttonTopPx}px`;
      this.$.forwardButtonContainer.style.top = `${buttonTopPx}px`;
    });
    observer.observe(this.$.carouselContainer);
    return observer;
  }

  private get columnOffsetWidth_(): number {
    if (this.$.slottedTable.assignedElements().length === 0) {
      return 0;
    }
    const tableElement = this.$.slottedTable.assignedElements()[0];
    const column = $$<HTMLElement>(tableElement, '.col');
    return column ? column.offsetWidth : 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'horizontal-carousel': HorizontalCarouselElement;
  }
}

customElements.define(HorizontalCarouselElement.is, HorizontalCarouselElement);
