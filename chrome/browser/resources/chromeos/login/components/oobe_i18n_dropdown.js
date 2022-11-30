// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/md_select.css.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {setupSelect} from './oobe_select.js';


/**
 * Languages/keyboard descriptor to display
 * @typedef {!OobeTypes.LanguageDsc|!OobeTypes.IMEDsc|!OobeTypes.DemoCountryDsc}
 */
var I18nMenuItem;

/**
 * Polymer class definition for 'oobe-i18n-dropdown'.
 * @polymer
 */
class OobeI18nDropdown extends PolymerElement {
  static get is() {
    return 'oobe-i18n-dropdown';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * List of languages/keyboards to display
       * @type {!Array<I18nMenuItem>}
       */
      items: {
        type: Array,
        observer: 'onItemsChanged_',
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

  constructor() {
    super();
    /**
     * Mapping from item id to item.
     * @type {Map<string,I18nMenuItem>}
     */
     this.idToItem_ = null;
  }

  focus() {
    this.$.select.focus();
  }

  /**
   * @param {string} value Option value.
   * @private
   */
  onSelected_(value) {
    const eventDetail = this.idToItem_.get(value);
    this.dispatchEvent(new CustomEvent('select-item',
        { detail: eventDetail, bubbles: true, composed: true }));
  }

  onItemsChanged_(items) {
    // Pass selection handler to setupSelect only during initial setup -
    // Otherwise, given that setupSelect does not remove previously registered
    // listeners, each new item list change would cause additional 'select-item'
    // events when selection changes.
    const selectionCallback =
        !this.idToItem_ ? this.onSelected_.bind(this) : null;
    this.idToItem_ = new Map();
    for (var i = 0; i < items.length; ++i) {
      var item = items[i];
      this.idToItem_.set(item.value, item);
    }
    setupSelect(this.$.select, items, selectionCallback);
  }
}

customElements.define(OobeI18nDropdown.is, OobeI18nDropdown);
