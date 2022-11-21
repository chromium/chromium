// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './categories.html.js';

export interface CategoriesElement {
  $: {
    backButton: HTMLButtonElement,
  };
}

export class CategoriesElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-categories';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private onCategoryClick_() {
    this.dispatchEvent(new CustomEvent<object>('category-select'));
  }

  private onBackClick_() {
    this.dispatchEvent(new Event('back-click'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-categories': CategoriesElement;
  }
}

customElements.define(CategoriesElement.is, CategoriesElement);
