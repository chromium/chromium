// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/icons.html.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './smart_search.css.js';
import {getHtml} from './smart_search.html.js';

export class SmartSearchElement extends CrLitElement {
  static get is() {
    return 'smart-search';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      searchQuery_: {type: String},
      smartSearchEnabled_: {type: Boolean},
    };
  }

  protected accessor searchQuery_: string = '';
  protected accessor smartSearchEnabled_: boolean =
      loadTimeData.getBoolean('kSmartSearch');

  protected onSearchInputKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      const input = e.target as HTMLInputElement;
      this.searchQuery_ = input.value;
    }
  }

  protected onSearchInput_(e: Event) {
    const input = e.target as HTMLInputElement;
    this.searchQuery_ = input.value;
  }

  protected onSearchClick_() {
    const input =
        this.shadowRoot?.querySelector<HTMLInputElement>('.search-input');
    if (input) {
      this.searchQuery_ = input.value;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'smart-search': SmartSearchElement;
  }
}

customElements.define(SmartSearchElement.is, SmartSearchElement);
