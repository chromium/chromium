// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';

import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './url_item.css.js';
import {getHtml} from './url_item.html.js';
import type {UrlItem} from './url_item_delegate.js';

export interface UrlItemElement {
  $: {
    favicon: HTMLElement,
    title: HTMLElement,
    closeButton: CrIconButtonElement,
  };
}

export class UrlItemElement extends CrLitElement {
  static get is() {
    return 'url-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      item: {type: Object},

      faviconImageSet_: {
        type: String,
        state: true,
      },
    };
  }

  accessor item: UrlItem = {id: -1, title: '', url: {url: ''}};
  protected accessor faviconImageSet_: string = '';

  override connectedCallback() {
    super.connectedCallback();

    this.role = 'article';
    this.tabIndex = 0;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('item')) {
      this.faviconImageSet_ = getFaviconForPageURL(
          this.item.url.url, /*isSyncedUrlForHistoryUi=*/ false);
    }
  }

  protected onCloseButtonClick_(e: Event) {
    e.stopPropagation();

    this.fire('close-button-click', this.item.id);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'url-item': UrlItemElement;
  }
}

customElements.define(UrlItemElement.is, UrlItemElement);
