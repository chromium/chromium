// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Polymer, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SearchService} from './search_service.js';
import {BrowserProxy} from './browser_proxy.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import {getInstance} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import 'chrome://resources/js/util.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './strings.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

  Polymer({
    is: 'downloads-toolbar',

    _template: html`{__html_template__}`,

    properties: {
      hasClearableDownloads: {
        type: Boolean,
        value: false,
        observer: 'updateClearAll_',
      },

      spinnerActive: {
        type: Boolean,
        notify: true,
      },
    },

    /** @private {?downloads.mojom.PageHandlerInterface} */
    mojoHandler_: null,

    /** @override */
    ready: function() {
      this.mojoHandler_ = BrowserProxy.getInstance().handler;
    },

    /** @return {boolean} Whether removal can be undone. */
    canUndo: function() {
      return !this.isSearchFocused();
    },

    /** @return {boolean} Whether "Clear all" should be allowed. */
    canClearAll: function() {
      return this.getSearchText().length == 0 && this.hasClearableDownloads;
    },

    /** @return {string} The full text being searched. */
    getSearchText: function() {
      return /** @type {!CrToolbarElement} */ (
          this.$.toolbar).getSearchField().getValue();
    },

    focusOnSearchInput: function() {
      return /** @type {!CrToolbarElement} */ (
          this.$.toolbar).getSearchField().showAndFocus();
    },

    isSearchFocused: function() {
      return /** @type {!CrToolbarElement} */ (
          this.$.toolbar).getSearchField().isSearchFocused();
    },

    /** @private */
    onClearAllTap_: function() {
      assert(this.canClearAll());
      this.mojoHandler_.clearAll();
      this.$.moreActionsMenu.close();
      getInstance().show(loadTimeData.getString('toastClearedAll'), true);
    },

    /** @private */
    onMoreActionsTap_: function() {
      this.$.moreActionsMenu.showAt(this.$.moreActions);
    },

    /**
     * @param {!CustomEvent<string>} event
     * @private
     */
    onSearchChanged_: function(event) {
      const searchService = SearchService.getInstance();
      if (searchService.search(event.detail)) {
        this.spinnerActive = searchService.isSearching();
      }
      this.updateClearAll_();
    },

    /** @private */
    onOpenDownloadsFolderTap_: function() {
      this.mojoHandler_.openDownloadsFolderRequiringGesture();
      this.$.moreActionsMenu.close();
    },

    /** @private */
    updateClearAll_: function() {
      this.$$('.clear-all').hidden = !this.canClearAll();
    },
  });
