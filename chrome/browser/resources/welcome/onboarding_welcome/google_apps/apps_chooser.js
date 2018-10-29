// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('nuxGoogleApps');

/**
 * @typedef {{
 *   id: number,
 *   name: string,
 *   icon: string,
 *   url: string,
 *   bookmarkId: ?string,
 *   selected: boolean,
 * }}
 */
nuxGoogleApps.AppItem;

/**
 * @typedef {{
 *   item: !nuxGoogleApps.AppItem,
 *   set: function(string, boolean):void
 * }}
 */
nuxGoogleApps.AppItemModel;

Polymer({
  is: 'apps-chooser',

  properties: {
    /**
     * @type {!Array<!nuxGoogleApps.AppItem>}
     * @private
     */
    appList_: Array,

    hasAppsSelected: {
      type: Boolean,
      notify: true,
      value: true,
    },
  },

  /** @private {nux.NuxGoogleAppsProxy} */
  appsProxy_: null,

  /** @private {nux.BookmarkProxy} */
  bookmarkProxy_: null,

  /** @override */
  ready() {
    this.appsProxy_ = nux.NuxGoogleAppsProxyImpl.getInstance();
    this.bookmarkProxy_ = nux.BookmarkProxyImpl.getInstance();
  },

  /** Called when bookmarks should be created for all selected apps. */
  populateAllBookmarks() {
    if (!this.appList_) {
      this.appsProxy_.getGoogleAppsList().then(list => {
        this.appList_ = list;
        this.appList_.forEach(app => {
          // Default select all items.
          app.selected = true;
          this.updateBookmark(app);
          // Icons only need to be added to the cache once.
          this.appsProxy_.cacheBookmarkIcon(app.id);
        });
      });
    }
  },

  /**
   * Returns an array of booleans for each selected app.
   * @return {!Array<boolean>}
   */
  getSelectedAppList() {
    if (this.appList_)
      return this.appList_.map(a => a.selected);
    else
      return [];
  },

  /**
   * @param {!nuxGoogleApps.AppItem} item
   * @private
   */
  updateBookmark(item) {
    if (item.selected && !item.bookmarkId) {
      this.bookmarkProxy_.toggleBookmarkBar(true);
      this.bookmarkProxy_.addBookmark(
          {
            title: item.name,
            url: item.url,
            parentId: '1',
          },
          result => {
            item.bookmarkId = result.id;
          });
    } else if (!item.selected && item.bookmarkId) {
      this.bookmarkProxy_.removeBookmark(item.bookmarkId);
      item.bookmarkId = null;
    }
  },

  /**
   * Handle toggling the apps selected.
   * @param {!{model: !nuxGoogleApps.AppItemModel}} e
   * @private
   */
  onAppClick_: function(e) {
    let item = e.model.item;
    e.model.set('item.selected', !item.selected);
    this.updateBookmark(item);
    this.hasAppsSelected = this.computeHasAppsSelected_();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAppPointerDown_: function(e) {
    e.currentTarget.classList.remove('keyboard-focused');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAppKeyUp_: function(e) {
    e.currentTarget.classList.add('keyboard-focused');
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasAppsSelected_: function() {
    return this.appList_ && this.appList_.some(a => a.selected);
  },
});
