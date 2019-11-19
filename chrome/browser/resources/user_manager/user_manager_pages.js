// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'user-manager-pages' is the element that controls paging in the
 * user manager screen.
 */
Polymer({
  is: 'user-manager-pages',

  properties: {
    /**
     * ID of the currently selected page.
     * @private
     */
    selectedPage_: String,

    /**
     * Data passed to the currently selected page.
     * @private {?Object}
     */
    pageData_: {type: Object, value: null}
  },

  listeners: {'change-page': 'onChangePage_'},

  /** @override */
  attached: function() {
    this.setSelectedPage('user-pods-page');
  },

  /**
   * Handler for the change-page event.
   * @param {Event} e The event containing ID of the page that is to be selected
   *     and the optional data to be passed to the page.
   * @private
   */
  onChangePage_: function(e) {
    this.setSelectedPage(e.detail.page, e.detail.data);
  },

  /**
   * Sets the selected page.
   * @param {string} pageId ID of the page that is to be selected.
   * @param {Object=} opt_pageData Optional data to be passed to the page.
   */
  setSelectedPage: function(pageId, opt_pageData) {
    this.pageData_ = opt_pageData || null;
    this.selectedPage_ = pageId;
    /** @type {CrViewManagerElement} */ (this.$.animatedPages)
        .switchView(this.selectedPage_);
  },

  /**
   * This is to prevent events from propagating to the document element, which
   * erroneously triggers user-pod selections.
   *
   * TODO(tangltom): re-examine if its necessary for user_pod_row.js to bind
   * listeners on the entire document element.
   *
   * @param {!Event} e
   * @private
   */
  stopPropagation_: function(e) {
    e.stopPropagation();
  },

  /** @return {boolean} */
  shouldShowCreateProfile_: function() {
    return this.selectedPage_ == 'create-user-page';
  }
});
