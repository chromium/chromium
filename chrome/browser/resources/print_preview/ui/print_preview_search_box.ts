// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {CrSearchFieldMixinLit} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {stripDiacritics} from 'chrome://resources/js/search_highlight_utils.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './print_preview_search_box.css.js';
import {getHtml} from './print_preview_search_box.html.js';

declare global {
  interface HTMLElementEventMap {
    'search-changed': CustomEvent<string>;
  }
}

const SANITIZE_REGEX: RegExp = /[-[\]{}()*+?.,\\^$|#\s]/g;

export interface PrintPreviewSearchBoxElement {
  $: {
    searchInput: CrInputElement,
  };
}

const PrintPreviewSearchBoxElementBase =
    CrSearchFieldMixinLit(WebUiListenerMixinLit(CrLitElement));

export class PrintPreviewSearchBoxElement extends
    PrintPreviewSearchBoxElementBase {
  static get is() {
    return 'print-preview-search-box';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      autofocus: {type: Boolean},

      searchQuery: {
        type: Object,
        notify: true,
      },
    };
  }

  override accessor autofocus: boolean = false;
  accessor searchQuery: RegExp|null = null;
  private lastQuery_: string = '';

  override firstUpdated() {
    this.addEventListener('search-changed', e => this.onSearchChanged_(e));
  }

  override getSearchInput(): CrInputElement {
    return this.$.searchInput;
  }

  override focus() {
    this.$.searchInput.focus();
  }

  private onSearchChanged_(e: CustomEvent<string>) {
    const strippedQuery = stripDiacritics(e.detail.trim());
    const safeQuery = strippedQuery.replace(SANITIZE_REGEX, '\\$&');
    if (safeQuery === this.lastQuery_) {
      return;
    }

    this.lastQuery_ = safeQuery;
    this.searchQuery =
        safeQuery.length > 0 ? new RegExp(`(${safeQuery})`, 'ig') : null;
  }

  protected onClearClick_() {
    this.setValue('');
    this.$.searchInput.focus();
  }
}

customElements.define(
    PrintPreviewSearchBoxElement.is, PrintPreviewSearchBoxElement);
