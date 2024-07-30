// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ChangePageOrigin} from './viewer_bookmark.js';
import {getCss} from './viewer_page_selector.css.js';
import {getHtml} from './viewer_page_selector.html.js';

export interface ViewerPageSelectorElement {
  $: {
    pageSelector: HTMLInputElement,
  };
}

export class ViewerPageSelectorElement extends CrLitElement {
  static get is() {
    return 'viewer-page-selector';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /** The number of pages the document contains. */
      docLength: {type: Number},

      /**
       * The current page being viewed (1-based). A change to pageNo is mirrored
       * immediately to the input field. A change to the input field is not
       * mirrored back until pageNoCommitted() is called and change-page is
       * fired.
       */
      pageNo: {type: Number},
    };
  }

  docLength: number = 1;
  pageNo: number = 1;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('docLength')) {
      const numDigits = this.docLength.toString().length;
      this.style.setProperty('--page-length-digits', `${numDigits}`);
    }
  }

  pageNoCommitted() {
    const page = parseInt(this.$.pageSelector.value, 10);

    if (!isNaN(page) && page <= this.docLength && page > 0) {
      this.dispatchEvent(new CustomEvent('change-page', {
        detail: {page: page - 1, origin: ChangePageOrigin.PAGE_SELECTOR},
        composed: true,
      }));
    } else {
      this.$.pageSelector.value = this.pageNo.toString();
    }
    this.$.pageSelector.blur();
  }

  select() {
    this.$.pageSelector.select();
  }

  /** @return True if the selector input field is currently focused. */
  isActive(): boolean {
    return this.shadowRoot!.activeElement === this.$.pageSelector;
  }

  /** Immediately remove any non-digit characters. */
  protected onInput_() {
    this.$.pageSelector.value = this.$.pageSelector.value.replace(/[^\d]/, '');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-page-selector': ViewerPageSelectorElement;
  }
}

customElements.define(ViewerPageSelectorElement.is, ViewerPageSelectorElement);
