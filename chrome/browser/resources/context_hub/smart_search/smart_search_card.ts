// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './smart_search_card.css.js';
import {getHtml} from './smart_search_card.html.js';

export interface SmartSearchResult {
  id: string;
  title: string;
  url: string;
  snippet: string;
}

export class SmartSearchCardElement extends CrLitElement {
  static get is() {
    return 'smart-search-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      result: {type: Object},
      selected: {type: Boolean, reflect: true},
    };
  }

  accessor result: SmartSearchResult|null = null;
  accessor selected: boolean = false;

  protected onCheckboxClick_(e: Event) {
    e.stopPropagation();
  }

  protected onCheckboxChange_(e: Event) {
    if (!this.result) {
      return;
    }
    const checkbox = e.target as HTMLElement & {checked: boolean};
    this.selected = checkbox.checked;
    this.fire('selection-change', {
      id: this.result.id,
      selected: checkbox.checked,
    });
  }

  protected onOpenUrlClick_() {
    if (this.result?.url) {
      window.open(this.result.url, '_blank');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'smart-search-card': SmartSearchCardElement;
  }
}

customElements.define(SmartSearchCardElement.is, SmartSearchCardElement);
