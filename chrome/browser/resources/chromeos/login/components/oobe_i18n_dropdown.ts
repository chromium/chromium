// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/md_select.css.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './oobe_i18n_dropdown.html.js';
import {OobeTypes} from './oobe_types.js';
import {setupSelect} from './oobe_select.js';

/**
 * Languages/keyboard descriptor to display
 */
export type I18nMenuItem =
    OobeTypes.LanguageDsc|OobeTypes.InputMethodsDsc|OobeTypes.DemoCountryDsc;

/**
 * Polymer class definition for 'oobe-i18n-dropdown'.
 */
class OobeI18nDropdown extends PolymerElement {
  static get is() {
    return 'oobe-i18n-dropdown' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * List of languages/keyboards to display
       */
      items: {
        type: Array,
        observer: 'onItemsChanged',
      },

      /**
       * ARIA-label for the selection menu.
       *
       * Note that we are not using "aria-label" property here, because
       * we want to pass the label value but not actually declare it as an
       * ARIA property anywhere but the actual target element.
       */
      labelForAria: String,
    };
  }

  private items: I18nMenuItem[];
  private labelforAria: string;
  private idToItem: Map<string,I18nMenuItem>|null;

  constructor() {
    super();
    /**
     * Mapping from item id to item.
     */
     this.idToItem = null;
  }

  override focus(): void {
    const select = this.shadowRoot?.querySelector('#select');
    assert(select instanceof HTMLElement);
    select.focus();
  }

  /**
   * @param value Option value.
   */
  private onSelected(value: string): void {
    const eventDetail = this.idToItem?.get(value);
    this.dispatchEvent(new CustomEvent('select-item',
        { detail: eventDetail, bubbles: true, composed: true }));
  }

  private onItemsChanged(items: I18nMenuItem[]): void {
    // Pass selection handler to setupSelect only during initial setup -
    // Otherwise, given that setupSelect does not remove previously registered
    // listeners, each new item list change would cause additional 'select-item'
    // events when selection changes.
    const selectionCallback =
        !this.idToItem ? this.onSelected.bind(this) : null;
    this.idToItem = new Map();
    for (let i = 0; i < items.length; ++i) {
      const item = items[i];
      this.idToItem.set(item.value, item);
    }
    const select = this.shadowRoot?.querySelector('#select');
    assert(select instanceof HTMLSelectElement);
    setupSelect(select, items, selectionCallback);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeI18nDropdown.is]: OobeI18nDropdown;
  }
}

customElements.define(OobeI18nDropdown.is, OobeI18nDropdown);
