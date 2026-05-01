/* Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_tooltip/cr_tooltip.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './carousel.css.js';
import {getHtml} from './carousel.html.js';

export interface SkillsCarouselElement {
  $: {
    carouselContainer: HTMLElement,
    itemsSlot: HTMLSlotElement,
    prevButton: HTMLElement,
    nextButton: HTMLElement,
  };
}

export class SkillsCarouselElement extends CrLitElement {
  static get is() {
    return 'skills-carousel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  private scroll_(direction: 'prev'|'next') {
    const items = this.$.itemsSlot.assignedElements() as HTMLElement[];
    if (items.length === 0) {
      return;
    }

    const computedStyle = window.getComputedStyle(this.$.carouselContainer);
    const cardWidth =
        parseFloat(computedStyle.getPropertyValue('--skills-card-width')) || 0;
    const gap =
        parseFloat(computedStyle.getPropertyValue('--skills-card-gap')) || 0;
    const itemWidth = cardWidth + gap;

    let firstVisibleItem = 0;
    if (itemWidth > 0) {
      firstVisibleItem =
          Math.ceil(Math.abs(this.$.carouselContainer.scrollLeft) / itemWidth);
      firstVisibleItem =
          Math.max(0, Math.min(items.length - 1, firstVisibleItem));
    }

    const itemsThatFit = itemWidth > 0 ?
        Math.floor(
            (this.$.carouselContainer.getBoundingClientRect().width + gap) /
            itemWidth) :
        0;

    const targetIndex = direction === 'prev' ?
        Math.max(0, firstVisibleItem - itemsThatFit) :
        Math.min(items.length - 1, firstVisibleItem + itemsThatFit);

    items[targetIndex]!.scrollIntoView({
      behavior: 'smooth',
      block: 'nearest',
      inline: 'start',
    });
  }

  protected onPrevClick_() {
    this.scroll_('prev');
  }

  protected onNextClick_() {
    this.scroll_('next');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'skills-carousel': SkillsCarouselElement;
  }
}

customElements.define(SkillsCarouselElement.is, SkillsCarouselElement);
