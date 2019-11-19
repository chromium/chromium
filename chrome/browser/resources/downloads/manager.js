// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import {Polymer, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy} from './browser_proxy.js';
import {States} from './constants.js';
import './item.js';
import {SearchService} from './search_service.js';
import './toolbar.js';
import 'chrome://resources/cr_components/managed_footnote/managed_footnote.m.js';
import 'chrome://resources/cr_elements/cr_page_host_style_css.m.js';
import {getInstance as getToastManagerInstance} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import {FindShortcutBehavior} from 'chrome://resources/js/find_shortcut_behavior.m.js';
import {queryRequiredElement} from 'chrome://resources/js/util.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';

  Polymer({
    is: 'downloads-manager',

    _template: html`{__html_template__}`,

    behaviors: [
      FindShortcutBehavior,
    ],

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

      /** @private {Element} */
      lastFocused_: Object,

      /** @private */
      listBlurred_: Boolean,
    },

    hostAttributes: {
      // TODO(dbeam): this should use a class instead.
      loading: true,
    },

    observers: [
      'itemsChanged_(items_.*)',
    ],

    /** @private {downloads.mojom.PageCallbackRouter} */
    mojoEventTarget_: null,

    /** @private {downloads.mojom.PageHandlerInterface} */
    mojoHandler_: null,

    /** @private {?SearchService} */
    searchService_: null,

    /** @private {!PromiseResolver} */
    loaded_: new PromiseResolver,

    /** @private {Array<number>} */
    listenerIds_: null,

    /** @private {?Function} */
    boundOnKeyDown_: null,

    /** @override */
    created: function() {
      const browserProxy = BrowserProxy.getInstance();
      this.mojoEventTarget_ = browserProxy.callbackRouter;
      this.mojoHandler_ = browserProxy.handler;
      this.searchService_ = SearchService.getInstance();

      // Regular expression that captures the leading slash, the content and the
      // trailing slash in three different groups.
      const CANONICAL_PATH_REGEX = /(^\/)([\/-\w]+)(\/$)/;
      const path = location.pathname.replace(CANONICAL_PATH_REGEX, '$1$2');
      if (path !== '/') {  // There are no subpages in chrome://downloads.
        window.history.replaceState(undefined /* stateObject */, '', '/');
      }
    },

    /** @override */
    attached: function() {
      document.documentElement.classList.remove('loading');
      this.listenerIds_ = [
        this.mojoEventTarget_.clearAll.addListener(this.clearAll_.bind(this)),
        this.mojoEventTarget_.insertItems.addListener(
            this.insertItems_.bind(this)),
        this.mojoEventTarget_.removeItem.addListener(
            this.removeItem_.bind(this)),
        this.mojoEventTarget_.updateItem.addListener(
            this.updateItem_.bind(this)),
      ];

      this.boundOnKeyDown_ = e => this.onKeyDown_(e);
      document.addEventListener('keydown', this.boundOnKeyDown_);

      this.loaded_.promise.then(() => {
        requestIdleCallback(function() {
          chrome.send(
              'metricsHandler:recordTime',
              ['Download.ResultsRenderedTime', window.performance.now()]);
          document.fonts.load('bold 12px Roboto');
        });
      });

      this.searchService_.loadMore();
    },

    /** @override */
    detached: function() {
      this.listenerIds_.forEach(
          id => assert(this.mojoEventTarget_.removeListener(id)));

      document.removeEventListener('keydown', this.boundOnKeyDown_);
      this.boundOnKeyDown_ = null;
    },

    /** @private */
    clearAll_: function() {
      this.set('items_', []);
    },

    /** @private */
    hasDownloadsChanged_: function() {
      if (this.hasDownloads_) {
        this.$.downloadsList.fire('iron-resize');
      }
    },

    /**
     * @param {number} index
     * @param {!Array<downloads.Data>} items
     * @private
     */
    insertItems_: function(index, items) {
      // Insert |items| at the given |index| via Array#splice().
      if (items.length > 0) {
        this.items_.splice.apply(this.items_, [index, 0].concat(items));
        this.updateHideDates_(index, index + items.length);
        this.notifySplices('items_', [{
                             index: index,
                             addedCount: items.length,
                             object: this.items_,
                             type: 'splice',
                             removed: [],
                           }]);
      }

      if (this.hasAttribute('loading')) {
        this.removeAttribute('loading');
        this.loaded_.resolve();
      }

      this.spinnerActive_ = false;
    },

    /** @private */
    itemsChanged_: function() {
      this.hasDownloads_ = this.items_.length > 0;
      this.$.toolbar.hasClearableDownloads =
          loadTimeData.getBoolean('allowDeletingHistory') &&
          this.items_.some(
              ({state}) => state != States.DANGEROUS &&
                  state != States.IN_PROGRESS &&
                  state != States.PAUSED);

      if (this.inSearchMode_) {
        IronA11yAnnouncer.requestAvailability();
        this.fire('iron-announce', {
          text: this.items_.length == 0 ?
              this.noDownloadsText_() :
              (this.items_.length == 1 ?
                   loadTimeData.getStringF(
                       'searchResultsSingular',
                       this.$.toolbar.getSearchText()) :
                   loadTimeData.getStringF(
                       'searchResultsPlural', this.items_.length,
                       this.$.toolbar.getSearchText()))
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
     * @param {!KeyboardEvent} e
     * @private
     */
    onKeyDown_: function(e) {
      let clearAllKey = 'c';
      // <if expr="is_macosx">
      // On Mac, pressing alt+c produces 'ç' as |event.key|.
      clearAllKey = 'ç';
      // </if>
      if (e.key === clearAllKey && e.altKey && !e.ctrlKey && !e.shiftKey &&
          !e.metaKey) {
        this.onClearAllCommand_();
        e.preventDefault();
        return;
      }

      if (e.key === 'z' && !e.altKey && !e.shiftKey) {
        let hasTriggerModifier = e.ctrlKey && !e.metaKey;
        // <if expr="is_macosx">
        hasTriggerModifier = !e.ctrlKey && e.metaKey;
        // </if>
        if (hasTriggerModifier) {
          this.onUndoCommand_();
          e.preventDefault();
        }
      }
    },

    /** @private */
    onClearAllCommand_() {
      if (!this.$.toolbar.canClearAll()) {
        return;
      }

      this.mojoHandler_.clearAll();
      getToastManagerInstance().show(
          loadTimeData.getString('toastClearedAll'), true);
    },

    /** @private */
    onUndoCommand_() {
      if (!this.$.toolbar.canUndo()) {
        return;
      }

      getToastManagerInstance().hide();
      this.mojoHandler_.undo();
    },

    /** @private */
    onScroll_: function() {
      const container = this.$.downloadsList.scrollTarget;
      const distanceToBottom =
          container.scrollHeight - container.scrollTop - container.offsetHeight;
      if (distanceToBottom <= 100) {
        // Approaching the end of the scrollback. Attempt to load more items.
        this.searchService_.loadMore();
      }
      this.hasShadow_ = container.scrollTop > 0;
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
      const removed = this.items_.splice(index, 1);
      this.updateHideDates_(index, index);
      this.notifySplices('items_', [{
                           index: index,
                           addedCount: 0,
                           object: this.items_,
                           type: 'splice',
                           removed: removed,
                         }]);
      this.onScroll_();
    },

    /** @private */
    onUndoClick_: function() {
      getToastManagerInstance().hide();
      this.mojoHandler_.undo();
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
        if (!current) {
          continue;
        }
        const prev = this.items_[i - 1];
        current.hideDate = !!prev && prev.dateString == current.dateString;
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

      this.notifyPath(`items_.${index}`);
      this.async(() => {
        const list = /** @type {!IronListElement} */ (this.$.downloadsList);
        list.updateSizeForIndex(index);
      });
    },

    // Override FindShortcutBehavior methods.
    handleFindShortcut: function(modalContextOpen) {
      if (modalContextOpen) {
        return false;
      }
      this.$.toolbar.focusOnSearchInput();
      return true;
    },

    // Override FindShortcutBehavior methods.
    searchInputHasFocus: function() {
      return this.$.toolbar.isSearchFocused();
    },
  });
