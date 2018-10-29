// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  const Manager = Polymer({
    is: 'downloads-manager',

    properties: {
      /** @private */
      hasDownloads_: {
        observer: 'hasDownloadsChanged_',
        type: Boolean,
      },

      /** @private */
      hasShadow_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** @private */
      inSearchMode_: {
        type: Boolean,
        value: false,
      },

      /** @private {!Array<!downloads.Data>} */
      items_: {
        type: Array,
        value: function() {
          return [];
        },
      },

      /** @private */
      spinnerActive_: {
        type: Boolean,
        notify: true,
      },
    },

    hostAttributes: {
      // TODO(dbeam): this should use a class instead.
      loading: true,
    },

    observers: [
      'itemsChanged_(items_.*)',
    ],

    /** @private {?downloads.BrowserProxy} */
    browserProxy_: null,

    /** @private {?downloads.SearchService} */
    searchService_: null,

    /** @private {!PromiseResolver} */
    loaded_: new PromiseResolver,

    /** @override */
    created: function() {
      this.browserProxy_ = downloads.BrowserProxy.getInstance();
      this.searchService_ = downloads.SearchService.getInstance();
    },

    /** @override */
    attached: function() {
      document.documentElement.classList.remove('loading');
    },

    /** @private */
    clearAll_: function() {
      this.set('items_', []);
    },

    /** @private */
    hasDownloadsChanged_: function() {
      if (loadTimeData.getBoolean('allowDeletingHistory'))
        this.$.toolbar.downloadsShowing = this.hasDownloads_;

      if (this.hasDownloads_)
        this.$.downloadsList.fire('iron-resize');
    },

    /**
     * @param {number} index
     * @param {!Array<!downloads.Data>} list
     * @private
     */
    insertItems_: function(index, list) {
      // Insert |list| at the given |index| via Array#splice().
      this.items_.splice.apply(this.items_, [index, 0].concat(list));
      this.updateHideDates_(index, index + list.length);
      this.notifySplices('items_', [{
                           index: index,
                           addedCount: list.length,
                           object: this.items_,
                           type: 'splice',
                           removed: [],
                         }]);

      if (this.hasAttribute('loading')) {
        this.removeAttribute('loading');
        this.loaded_.resolve();
      }

      this.spinnerActive_ = false;
    },

    /** @private */
    itemsChanged_: function() {
      this.hasDownloads_ = this.items_.length > 0;

      if (this.inSearchMode_) {
        Polymer.IronA11yAnnouncer.requestAvailability();
        this.fire('iron-announce', {
          text: this.hasDownloads_ ?
              loadTimeData.getStringF(
                  'searchResultsFor', this.$.toolbar.getSearchText()) :
              this.noDownloadsText_()
        });
      }
    },

    /**
     * @return {string} The text to show when no download items are showing.
     * @private
     */
    noDownloadsText_: function() {
      return loadTimeData.getString(
          this.inSearchMode_ ? 'noSearchResults' : 'noDownloads');
    },

    /**
     * @param {Event} e
     * @private
     */
    onCanExecute_: function(e) {
      e = /** @type {cr.ui.CanExecuteEvent} */ (e);
      switch (e.command.id) {
        case 'undo-command':
          e.canExecute = this.$.toolbar.canUndo();
          break;
        case 'clear-all-command':
          e.canExecute = this.$.toolbar.canClearAll();
          break;
        case 'find-command':
          e.canExecute = true;
          break;
      }
    },

    /**
     * @param {Event} e
     * @private
     */
    onCommand_: function(e) {
      if (e.command.id == 'clear-all-command')
        this.browserProxy_.clearAll();
      else if (e.command.id == 'undo-command')
        this.browserProxy_.undo();
      else if (e.command.id == 'find-command')
        this.$.toolbar.onFindCommand();
    },

    /** @private */
    onListScroll_: function() {
      const list = this.$.downloadsList;
      if (list.scrollHeight - list.scrollTop - list.offsetHeight <= 100) {
        // Approaching the end of the scrollback. Attempt to load more items.
        this.searchService_.loadMore();
      }
      this.hasShadow_ = list.scrollTop > 0;
    },

    /**
     * @return {!Promise}
     * @private
     */
    onLoad_: function() {
      cr.ui.decorate('command', cr.ui.Command);
      document.addEventListener('canExecute', this.onCanExecute_.bind(this));
      document.addEventListener('command', this.onCommand_.bind(this));

      this.searchService_.loadMore();
      return this.loaded_.promise;
    },

    /** @private */
    onSearchChanged_: function() {
      this.inSearchMode_ = this.searchService_.isSearching();
    },

    /**
     * @param {number} index
     * @private
     */
    removeItem_: function(index) {
      let removed = this.items_.splice(index, 1);
      this.updateHideDates_(index, index);
      this.notifySplices('items_', [{
                           index: index,
                           addedCount: 0,
                           object: this.items_,
                           type: 'splice',
                           removed: removed,
                         }]);
      this.onListScroll_();
    },

    /**
     * Updates whether dates should show for |this.items_[start - end]|. Note:
     * this method does not trigger template bindings. Use notifySplices() or
     * after calling this method to ensure items are redrawn.
     * @param {number} start
     * @param {number} end
     * @private
     */
    updateHideDates_: function(start, end) {
      for (let i = start; i <= end; ++i) {
        const current = this.items_[i];
        if (!current)
          continue;
        const prev = this.items_[i - 1];
        current.hideDate = !!prev && prev.date_string == current.date_string;
      }
    },

    /**
     * @param {number} index
     * @param {!downloads.Data} data
     * @private
     */
    updateItem_: function(index, data) {
      this.items_[index] = data;
      this.updateHideDates_(index, index);
      this.notifySplices('items_', [{
                           index: index,
                           addedCount: 0,
                           object: this.items_,
                           type: 'splice',
                           removed: [],
                         }]);
      this.async(() => {
        const list = /** @type {!IronListElement} */ (this.$.downloadsList);
        list.updateSizeForIndex(index);
      });
    },
  });

  Manager.clearAll = function() {
    Manager.get().clearAll_();
  };

  /** @return {!downloads.Manager} */
  Manager.get = function() {
    return /** @type {!downloads.Manager} */ (
        queryRequiredElement('downloads-manager'));
  };

  Manager.insertItems = function(index, list) {
    Manager.get().insertItems_(index, list);
  };

  /** @return {!Promise} */
  Manager.onLoad = function() {
    return Manager.get().onLoad_();
  };

  Manager.removeItem = function(index) {
    Manager.get().removeItem_(index);
  };

  Manager.updateItem = function(index, data) {
    Manager.get().updateItem_(index, data);
  };

  return {Manager: Manager};
});
