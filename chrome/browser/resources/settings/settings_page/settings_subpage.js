// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-subpage' shows a subpage beneath a subheader. The header contains
 * the subpage title, a search field and a back icon.
 */

Polymer({
  is: 'settings-subpage',

  behaviors: [
    FindShortcutBehavior,
    Polymer.IronResizableBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    pageTitle: String,

    titleIcon: String,

    learnMoreUrl: String,

    /** Setting a |searchLabel| will enable search. */
    searchLabel: String,

    searchTerm: {
      type: String,
      notify: true,
      value: '',
    },

    /** If true shows an active spinner at the end of the subpage header. */
    showSpinner: {
      type: Boolean,
      value: false,
    },

    /**
     * Title (i.e., tooltip) to be displayed on the spinner. If |showSpinner| is
     * false, this field has no effect.
     */
    spinnerTitle: {
      type: String,
      value: '',
    },

    /**
     * Indicates which element triggers this subpage. Used by the searching
     * algorithm to show search bubbles. It is |null| for subpages that are
     * skipped during searching.
     * @type {?HTMLElement}
     */
    associatedControl: {
      type: Object,
      value: null,
    },

    /** @private */
    active_: {
      type: Boolean,
      value: false,
      observer: 'onActiveChanged_',
    },
  },

  /** @private {boolean} */
  lastActiveValue_: false,

  // Override FindShortcutBehavior property.
  findShortcutListenOnAttach: false,

  /** @override */
  attached: function() {
    if (this.searchLabel) {
      // |searchLabel| should not change dynamically.
      this.listen(this, 'clear-subpage-search', 'onClearSubpageSearch_');
    }
  },

  /** @override */
  detached: function() {
    if (this.searchLabel) {
      // |searchLabel| should not change dynamically.
      this.unlisten(this, 'clear-subpage-search', 'onClearSubpageSearch_');
    }
  },

  /** Focuses the back button when page is loaded. */
  initialFocus: function() {
    Polymer.RenderStatus.afterNextRender(
        this, () => cr.ui.focusWithoutInk(this.$.closeButton));
  },

  /** @protected */
  currentRouteChanged: function(route) {
    this.active_ = this.getAttribute('route-path') == route.path;
  },

  /** @private */
  onActiveChanged_: function() {
    if (this.lastActiveValue_ == this.active_) {
      return;
    }
    this.lastActiveValue_ = this.active_;

    if (this.active_ && this.pageTitle) {
      document.title =
          loadTimeData.getStringF('settingsAltPageTitle', this.pageTitle);
    }

    if (!this.searchLabel) {
      return;
    }

    const searchField = this.$$('cr-search-field');
    if (searchField) {
      searchField.setValue('');
    }

    if (this.active_) {
      this.becomeActiveFindShortcutListener();
    } else {
      this.removeSelfAsFindShortcutListener();
    }
  },

  /**
   * Clear the value of the search field.
   * @param {!Event} e
   */
  onClearSubpageSearch_: function(e) {
    e.stopPropagation();
    this.$$('cr-search-field').setValue('');
  },

  /** @private */
  onTapBack_: function() {
    settings.navigateToPreviousRoute();
  },

  /** @private */
  onSearchChanged_: function(e) {
    this.searchTerm = e.detail;
  },

  // Override FindShortcutBehavior methods.
  handleFindShortcut: function(modalContextOpen) {
    if (modalContextOpen) {
      return false;
    }
    this.$$('cr-search-field').getSearchInput().focus();
    return true;
  },

  // Override FindShortcutBehavior methods.
  searchInputHasFocus: function() {
    const field = this.$$('cr-search-field');
    return field.getSearchInput() == field.shadowRoot.activeElement;
  },
});
