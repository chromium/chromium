// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

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

  async setDelegate(delegate: UrlItemDelegate) {
    assert(this.delegate_ === null);

    this.delegate_ = delegate;

    this.items_ = await this.delegate_.getItems();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'url-item-grid': UrlItemGrid;
  }
}

customElements.define(UrlItemGrid.is, UrlItemGrid);
