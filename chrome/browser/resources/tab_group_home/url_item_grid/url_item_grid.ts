// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ItemEventType} from './url_item_delegate.js';
import type {UrlItem, UrlItemDelegate} from './url_item_delegate.js';
import {getCss} from './url_item_grid.css.js';
import {getHtml} from './url_item_grid.html.js';

export class UrlItemGrid extends CrLitElement {
  static get is() {
    return 'url-item-grid';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      items_: {type: Array},
    };
  }

  protected accessor items_: UrlItem[] = [];
  private delegate_: UrlItemDelegate|null = null;
  private eventTracker_ = new EventTracker();

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  async setDelegate(delegate: UrlItemDelegate) {
    assert(this.delegate_ === null);

    this.delegate_ = delegate;

    const eventTarget = this.delegate_.getEventTarget();
    this.eventTracker_.add(
        eventTarget, ItemEventType.ITEM_ADDED, this.onItemAdded_.bind(this));
    this.eventTracker_.add(
        eventTarget, ItemEventType.ITEM_REMOVED,
        this.onItemRemoved_.bind(this));
    this.eventTracker_.add(
        eventTarget, ItemEventType.ITEM_MOVED, this.onItemMoved_.bind(this));

    this.items_ = await this.delegate_.getItems();
  }

  private onItemAdded_(e: CustomEvent<{item: UrlItem, index: number}>) {
    this.items_.splice(e.detail.index, 0, e.detail.item);
    this.requestUpdate();
  }

  private onItemMoved_(e: CustomEvent<{id: number, newIndex: number}>) {
    const oldIndex = this.items_.findIndex(item => item.id === e.detail.id);
    assert(oldIndex !== -1);

    const [itemToMove] = this.items_.splice(oldIndex, 1);
    this.items_.splice(e.detail.newIndex, 0, itemToMove!);
    this.requestUpdate();
  }

  private onItemRemoved_(e: CustomEvent<number>) {
    const index = this.items_.findIndex(item => item.id === e.detail);
    assert(index !== -1);

    this.items_.splice(index, 1);
    this.requestUpdate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'url-item-grid': UrlItemGrid;
  }
}

customElements.define(UrlItemGrid.is, UrlItemGrid);
