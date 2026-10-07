// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'category-reference-card' is a card that shows a list of
 * chips related to a certain category.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {YourSavedInfoDataCategory, YourSavedInfoDataChip} from '../metrics_browser_proxy.js';

import type {DataChip} from './autofill_page.js';
import {getCss} from './category_reference_card.css.js';
import {getHtml} from './category_reference_card.html.js';

export type DataChipClickEvent = CustomEvent<{
  chipId: YourSavedInfoDataChip,
}>;

export type DataCategoryClickEvent = CustomEvent<{
  categoryId: YourSavedInfoDataCategory,
}>;

declare global {
  interface HTMLElementEventMap {
    'data-chip-click': DataChipClickEvent;
    'data-category-click': DataCategoryClickEvent;
  }
}

export class CategoryReferenceCardElement extends CrLitElement {
  static get is() {
    return 'category-reference-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      cardTitle: {type: String},
      categoryId: {type: Number},
      chips: {type: Array},
      isExternal: {type: Boolean},
    };
  }

  accessor cardTitle: string = '';
  accessor categoryId: YourSavedInfoDataCategory;
  accessor chips: DataChip[] = [];
  accessor isExternal: boolean = false;

  protected onDataCategoryClick_() {
    this.fire('data-category-click', {categoryId: this.categoryId});
  }

  protected onDataChipClick_(event: Event) {
    const target = event.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    const chip = this.chips[index];
    this.fire('data-chip-click', {chipId: chip.id});
  }

  override focus() {
    this.shadowRoot.querySelector('cr-link-row')!.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'category-reference-card': CategoryReferenceCardElement;
  }
}

customElements.define(
    CategoryReferenceCardElement.is, CategoryReferenceCardElement);
