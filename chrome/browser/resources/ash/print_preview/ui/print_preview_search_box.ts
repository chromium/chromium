// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import './print_preview_shared.css.js';

import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {CrSearchFieldMixin} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {stripDiacritics} from 'chrome://resources/js/search_highlight_utils.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './print_preview_search_box.html.js';

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
    CrSearchFieldMixin(WebUiListenerMixin(PolymerElement));

export class PrintPreviewSearchBoxElement extends
    PrintPreviewSearchBoxElementBase {
  static get is() {
    return 'print-preview-search-box';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      autofocus: Boolean,

      searchQuery: {
        type: Object,
        notify: true,
      },
    };
  }

  override autofocus: boolean;
  searchQuery: RegExp|null;
  private lastQuery_: string = '';

  override ready() {
    super.ready();

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

  private onClearClick_() {
    this.setValue('');
    this.$.searchInput.focus();
  }
}

customElements.define(
    PrintPreviewSearchBoxElement.is, PrintPreviewSearchBoxElement);
