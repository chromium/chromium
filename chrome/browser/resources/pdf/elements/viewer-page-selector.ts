// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared-vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ChangePageOrigin} from './viewer-bookmark.js';
import {getTemplate} from './viewer-page-selector.html.js';

export interface ViewerPageSelectorElement {
  $: {
    pageSelector: HTMLInputElement,
  };
}

export class ViewerPageSelectorElement extends PolymerElement {
  static get is() {
    return 'viewer-page-selector';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The number of pages the document contains. */
      docLength: {type: Number, value: 1, observer: 'docLengthChanged_'},

      /**
       * The current page being viewed (1-based). A change to pageNo is mirrored
       * immediately to the input field. A change to the input field is not
       * mirrored back until pageNoCommitted() is called and change-page is
       * fired.
       */
      pageNo: {
        type: Number,
        value: 1,
      },
    };
  }

  docLength: number;
  pageNo: number;

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

  private docLengthChanged_() {
    const numDigits = this.docLength.toString().length;
    this.style.setProperty('--page-length-digits', `${numDigits}`);
  }

  select() {
    this.$.pageSelector.select();
  }

  /** @return True if the selector input field is currently focused. */
  isActive(): boolean {
    return this.shadowRoot!.activeElement === this.$.pageSelector;
  }

  /** Immediately remove any non-digit characters. */
  private onInput_() {
    this.$.pageSelector.value = this.$.pageSelector.value.replace(/[^\d]/, '');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-page-selector': ViewerPageSelectorElement;
  }
}

customElements.define(ViewerPageSelectorElement.is, ViewerPageSelectorElement);
