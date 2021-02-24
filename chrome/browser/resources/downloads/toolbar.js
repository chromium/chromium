// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/util.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './strings.m.js';

import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {Data} from './data.js';
import {PageHandlerInterface} from './downloads.mojom-webui.js';
import {SearchService} from './search_service.js';

Polymer({
  is: 'downloads-toolbar',

  _template: html`{__html_template__}`,

  properties: {
    hasClearableDownloads: {
      type: Boolean,
      value: false,
      observer: 'updateClearAll_',
    },

    /** @type {!Array<!Data>} */
    items: {
      type: Array,
      value: Array,
    },

    spinnerActive: {
      type: Boolean,
      notify: true,
    },
  },

  /** @private {?PageHandlerInterface} */
  mojoHandler_: null,

  /** @override */
  ready() {
    this.mojoHandler_ = BrowserProxy.getInstance().handler;
  },

  /** @return {boolean} Whether removal can be undone. */
  canUndo() {
    return !this.isSearchFocused();
  },

  /** @return {boolean} Whether "Clear all" should be allowed. */
  canClearAll() {
    return this.getSearchText().length === 0 && this.hasClearableDownloads;
  },

  /** @return {string} The full text being searched. */
  getSearchText() {
    return /** @type {!CrToolbarElement} */ (this.$.toolbar)
        .getSearchField()
        .getValue();
  },

  focusOnSearchInput() {
    return /** @type {!CrToolbarElement} */ (this.$.toolbar)
        .getSearchField()
        .showAndFocus();
  },

  isSearchFocused() {
    return /** @type {!CrToolbarElement} */ (this.$.toolbar)
        .getSearchField()
        .isSearchFocused();
  },

  /** @private */
  onClearAllTap_() {
    assert(this.canClearAll());
    this.mojoHandler_.clearAll();
    this.$.moreActionsMenu.close();
    const canUndo =
        this.items.some(data => !data.isDangerous && !data.isMixedContent);
    getToastManager().show(loadTimeData.getString('toastClearedAll'),
        /* hideSlotted= */ !canUndo);
  },

  /** @private */
  onMoreActionsTap_() {
    this.$.moreActionsMenu.showAt(this.$.moreActions);
  },

  /**
   * @param {!CustomEvent<string>} event
   * @private
   */
  onSearchChanged_(event) {
    const searchService = SearchService.getInstance();
    if (searchService.search(event.detail)) {
      this.spinnerActive = searchService.isSearching();
    }
    this.updateClearAll_();
  },

  /** @private */
  onOpenDownloadsFolderTap_() {
    this.mojoHandler_.openDownloadsFolderRequiringGesture();
    this.$.moreActionsMenu.close();
  },

  /** @private */
  updateClearAll_() {
    this.$$('.clear-all').hidden = !this.canClearAll();
  },
});
