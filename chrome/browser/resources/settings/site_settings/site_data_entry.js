// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-data-entry' handles showing the local storage summary for a site.
 */

Polymer({
  is: 'site-data-entry',

  behaviors: [
    FocusRowBehavior,
    I18nBehavior,
  ],

  properties: {
    /** @type {!CookieDataSummaryItem} */
    model: Object,
  },

  /** @private {settings.LocalDataBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.LocalDataBrowserProxyImpl.getInstance();
  },

  /**
   * Deletes all site data for this site.
   * @param {!Event} e
   * @private
   */
  onRemove_: function(e) {
    e.stopPropagation();
    this.browserProxy_.removeItem(this.model.site);
  },
});
