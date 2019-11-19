// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Polymer class definition for 'oobe-i18n-dropdown'.
 */
/**
 * Languages/keyboard descriptor to display
 * @typedef {!OobeTypes.LanguageDsc|!OobeTypes.IMEDsc|!OobeTypes.DemoCountryDsc}
 */
var I18nMenuItem;

Polymer({
  is: 'oobe-i18n-dropdown',

  properties: {
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
  },

  /**
   * Mapping from item id to item.
   * @type {Map<string,I18nMenuItem>}
   */
  idToItem_: null,

  focus: function() {
    this.$.select.focus();
  },

  /**
   * @param {string} value Option value.
   * @private
   */
  onSelected_: function(value) {
    this.fire('select-item', this.idToItem_.get(value));
  },

  onItemsChanged_: function(items) {
    // Pass selection handler to setupSelect only during initial setup -
    // Otherwise, given that setupSelect does not remove previously registered
    // listeners, each new item list change would cause additional 'select-item'
    // events when selection changes.
    let selectionCallback =
        !this.idToItem_ ? this.onSelected_.bind(this) : null;
    this.idToItem_ = new Map();
    for (var i = 0; i < items.length; ++i) {
      var item = items[i];
      this.idToItem_.set(item.value, item);
    }
    setupSelect(this.$.select, items, selectionCallback);
  },
});
