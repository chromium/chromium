// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Polymer, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.m.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.m.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_selection_overlay.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import './shared_style.js';
import './strings.m.js';
import {Command, MenuSource} from './constants.js';
import {CommandManager} from './command_manager.js';
import {StoreClient} from './store_client.js';
import {deselectItems, setSearchTerm} from './actions.js';

Polymer({
  is: 'bookmarks-toolbar',

  _template: html`{__html_template__}`,

  behaviors: [
    StoreClient,
  ],

  properties: {
    sidebarWidth: {
      type: String,
      observer: 'onSidebarWidthChanged_',
    },

    showSelectionOverlay: {
      type: Boolean,
      computed: 'shouldShowSelectionOverlay_(selectedItems_, globalCanEdit_)',
      readOnly: true,
    },

    /** @private */
    narrow_: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @private */
    searchTerm_: {
      type: String,
      observer: 'onSearchTermChanged_',
    },

    /** @private {!Set<string>} */
    selectedItems_: Object,

    /** @private */
    globalCanEdit_: Boolean,
  },

  attached: function() {
    this.watch('searchTerm_', function(state) {
      return state.search.term;
    });
    this.watch('selectedItems_', function(state) {
      return state.selection.items;
    });
    this.watch('globalCanEdit_', function(state) {
      return state.prefs.canEdit;
    });
    this.updateFromStore();
  },

  /** @return {CrToolbarSearchFieldElement} */
  get searchField() {
    return /** @type {CrToolbarElement} */ (this.$$('cr-toolbar'))
        .getSearchField();
  },

  /**
   * @param {Event} e
   * @private
   */
  onMenuButtonOpenTap_: function(e) {
    this.fire('open-command-menu', {
      targetElement: e.target,
      source: MenuSource.TOOLBAR,
    });
  },

  /** @private */
  onDeleteSelectionTap_: function() {
    const selection = this.selectedItems_;
    const commandManager = CommandManager.getInstance();
    assert(commandManager.canExecute(Command.DELETE, selection));
    commandManager.handle(Command.DELETE, selection);
  },

  /** @private */
  onClearSelectionTap_: function() {
    this.dispatch(deselectItems());
  },

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onSearchChanged_: function(e) {
    if (e.detail != this.searchTerm_) {
      this.dispatch(setSearchTerm(e.detail));
    }
  },

  /** @private */
  onSidebarWidthChanged_: function() {
    this.style.setProperty('--sidebar-width', this.sidebarWidth);
  },

  /** @private */
  onSearchTermChanged_: function() {
    this.searchField.setValue(this.searchTerm_ || '');
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSelectionOverlay_: function() {
    return this.selectedItems_.size > 1 && this.globalCanEdit_;
  },

  /**
   * @return {boolean}
   * @private
   */
  canDeleteSelection_: function() {
    return this.showSelectionOverlay &&
        CommandManager.getInstance().canExecute(
            Command.DELETE, this.selectedItems_);
  },

  /**
   * @return {string}
   * @private
   */
  getItemsSelectedString_: function() {
    return loadTimeData.getStringF('itemsSelected', this.selectedItems_.size);
  },
});
