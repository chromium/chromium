// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './topic_collection_carousel.css.js';
import {getHtml} from './topic_collection_carousel.html.js';
import type {Collection} from './topic_utils.js';

// Size of the favicon drawn in a card's image area.
const CARD_FAVICON_SIZE = 32;

export interface TopicCollectionCarouselElement {
  $: {
    cards: HTMLElement,
  };
}

// A titled, horizontally scrolling row of cards, one per page of a topic's
// collection. Clicking a card opens its page in a new tab. The back and forward
// buttons scroll by a page of cards, wrapping around at either end, and only
// show when the cards overflow.
export class TopicCollectionCarouselElement extends CrLitElement {
  static get is() {
    return 'topic-collection-carousel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      collection: {type: Object},
      canScrollBack_: {type: Boolean, state: true},
      canScrollForward_: {type: Boolean, state: true},
    };
  }

  accessor collection: Collection|null = null;
  protected accessor canScrollBack_: boolean = false;
  protected accessor canScrollForward_: boolean = false;

  private resizeObserver_: ResizeObserver|null = null;

  override connectedCallback() {
    super.connectedCallback();
    // Fires once when observation starts too, which sets the initial state.
    this.resizeObserver_ =
        new ResizeObserver(() => this.updateScrollButtons_());
    this.updateComplete.then(() => {
      this.resizeObserver_?.observe(this.$.cards);
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.resizeObserver_?.disconnect();
    this.resizeObserver_ = null;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('collection')) {
      this.updateScrollButtons_();
    }
  }

  // TODO(crbug.com/558572977): Show the page's image from `PageImageService`
  // (see `cr-history-embeddings-result-image`), keeping the favicon as the
  // fallback. That needs a new `ClientId`, which needs launch approval.
  protected getFavicon_(url: string): string {
    return getFaviconForPageURL(
        url, /*isSyncedUrlForHistoryUi=*/ false, /*remoteIconUrlForUma=*/ '',
        CARD_FAVICON_SIZE);
  }

  protected onCardClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const item = this.collection?.items[index];
    if (item) {
      OpenWindowProxyImpl.getInstance().openUrl(item.url);
    }
  }

  protected onBackClick_() {
    this.scrollByPage_(-1);
  }

  protected onForwardClick_() {
    this.scrollByPage_(1);
  }

  protected onCardsScroll_() {
    this.updateScrollButtons_();
  }

  private isRtl_(): boolean {
    return getComputedStyle(this).direction === 'rtl';
  }

  // Scrolls one visible width of cards back (-1) or forward (1), wrapping
  // around to the other end when already at the end it's scrolling towards.
  private scrollByPage_(direction: number) {
    const cards = this.$.cards;
    const sign = this.isRtl_() ? -1 : 1;
    const atEnd =
        direction > 0 ? !this.canScrollForward_ : !this.canScrollBack_;
    if (atEnd) {
      const lastPage = cards.scrollWidth - cards.clientWidth;
      const left = direction > 0 ? 0 : sign * lastPage;
      cards.scrollTo({left, behavior: 'smooth'});
      return;
    }
    cards.scrollBy(
        {left: sign * direction * cards.clientWidth, behavior: 'smooth'});
  }

  private updateScrollButtons_() {
    const cards = this.$.cards;
    // `scrollLeft` is 0 at the start, and negative when scrolled in RTL.
    const scrolled = Math.abs(cards.scrollLeft);
    // Allow a pixel for fractional scroll positions.
    this.canScrollBack_ = scrolled > 1;
    this.canScrollForward_ =
        scrolled + cards.clientWidth < cards.scrollWidth - 1;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-collection-carousel': TopicCollectionCarouselElement;
  }
}

customElements.define(
    TopicCollectionCarouselElement.is, TopicCollectionCarouselElement);
