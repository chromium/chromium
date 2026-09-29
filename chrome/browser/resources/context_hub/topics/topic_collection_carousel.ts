// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from '../context_hub.mojom-webui.js';

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
      imageUrls_: {type: Object, state: true},
    };
  }

  accessor collection: Collection|null = null;
  protected accessor canScrollBack_: boolean = false;
  protected accessor canScrollForward_: boolean = false;
  // Image URLs of the cards' pages, keyed by page URL. Pages without one, or
  // whose image failed to load, show their favicon instead.
  protected accessor imageUrls_: Map<string, string> = new Map();

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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('collection')) {
      this.imageUrls_ = new Map();
      this.fetchImages_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('collection')) {
      this.updateScrollButtons_();
    }
  }

  protected getImageUrl_(url: string): string {
    return this.imageUrls_.get(url) || '';
  }

  protected getFavicon_(url: string): string {
    return getFaviconForPageURL(
        url, /*isSyncedUrlForHistoryUi=*/ false, /*remoteIconUrlForUma=*/ '',
        CARD_FAVICON_SIZE);
  }

  // Falls back to the favicon when a page's image can't be loaded.
  protected onCardImageError_(e: Event) {
    const url = (e.currentTarget as HTMLElement).dataset['url'];
    if (url && this.imageUrls_.has(url)) {
      const imageUrls = new Map(this.imageUrls_);
      imageUrls.delete(url);
      this.imageUrls_ = imageUrls;
    }
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

  // Requests the image of each card's page and applies them all in a single
  // update. Replies for a collection that has since been replaced are dropped.
  private async fetchImages_() {
    const collection = this.collection;
    if (!collection) {
      return;
    }
    const handler = browserProxyFactory.getInstance().handler;
    const urls = [...new Set(collection.items.map(item => item.url))];
    const replies =
        await Promise.all(urls.map(url => handler.getTopicPageImageUrl(url)));
    if (this.collection !== collection) {
      return;
    }
    const imageUrls = new Map<string, string>();
    replies.forEach(({imageUrl}, i) => {
      if (imageUrl) {
        imageUrls.set(urls[i]!, imageUrl);
      }
    });
    this.imageUrls_ = imageUrls;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-collection-carousel': TopicCollectionCarouselElement;
  }
}

customElements.define(
    TopicCollectionCarouselElement.is, TopicCollectionCarouselElement);
