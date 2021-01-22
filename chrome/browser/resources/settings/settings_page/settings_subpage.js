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
    I18nBehavior,
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
     * Whether we should hide the "close" button to get to the previous page.
     */
    hideCloseButton: {
      type: Boolean,
      value: false,
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

    /**
     * Whether the subpage search term should be preserved across navigations.
     */
    preserveSearchTerm: {
      type: Boolean,
      value: false,
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
  attached() {
    if (this.searchLabel) {
      // |searchLabel| should not change dynamically.
      this.listen(this, 'clear-subpage-search', 'onClearSubpageSearch_');
    }
  },

  /** @override */
  detached() {
    if (this.searchLabel) {
      // |searchLabel| should not change dynamically.
      this.unlisten(this, 'clear-subpage-search', 'onClearSubpageSearch_');
    }
  },

  /**
   * @return {!Promise<!CrSearchFieldElement>}
   * @private
   */
  getSearchField_() {
    let searchField = this.$$('cr-search-field');
    if (searchField) {
      return Promise.resolve(searchField);
    }

    return new Promise(resolve => {
      listenOnce(this, 'dom-change', () => {
        searchField = this.$$('cr-search-field');
        resolve(assert(searchField));
      });
    });
  },

  /**
   * Restore search field value from URL search param
   * @private
   */
  restoreSearchInput_() {
    const searchField = this.$$('cr-search-field');
    if (assert(searchField)) {
      const urlSearchQuery =
          settings.Router.getInstance().getQueryParameters().get(
              'searchSubpage') ||
          '';
      this.searchTerm = urlSearchQuery;
      searchField.setValue(urlSearchQuery);
    }
  },

  /**
   * Preserve search field value to URL search param
   * @private
   */
  preserveSearchInput_() {
    const query = this.searchTerm;
    const searchParams = query.length > 0 ?
        new URLSearchParams('searchSubpage=' + encodeURIComponent(query)) :
        undefined;
    const currentRoute = settings.Router.getInstance().getCurrentRoute();
    settings.Router.getInstance().navigateTo(currentRoute, searchParams);
  },

  /** Focuses the back button when page is loaded. */
  focusBackButton() {
    if (this.hideCloseButton) {
      return;
    }
    Polymer.RenderStatus.afterNextRender(
        this, () => cr.ui.focusWithoutInk(this.$.closeButton));
  },

  /** @protected */
  currentRouteChanged(newRoute, oldRoute) {
    this.active_ = this.getAttribute('route-path') === newRoute.path;
    if (this.active_ && this.searchLabel && this.preserveSearchTerm) {
      this.getSearchField_().then(() => this.restoreSearchInput_());
    }
    // <if expr="chromeos">
    if (!oldRoute && loadTimeData.valueExists('isOSSettings') &&
        loadTimeData.getBoolean('isOSSettings') && !getSettingIdParameter()) {
      // If an OS settings subpage is opened directly (i.e the |oldRoute| is
      // null, e.g via an OS settings search result that surfaces from the
      // Chrome OS launcher), the back button should be focused since it's the
      // first actionable element in the the subpage. An exception is when
      // a setting is deep linked, focus that setting instead of back button.
      this.focusBackButton();
    }
    // </if>
  },

  /** @private */
  onActiveChanged_() {
    if (this.lastActiveValue_ === this.active_) {
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
  onClearSubpageSearch_(e) {
    e.stopPropagation();
    this.$$('cr-search-field').setValue('');
  },

  /** @private */
  onBackClick_() {
    settings.Router.getInstance().navigateToPreviousRoute();
  },

  /** @private */
  onHelpClick_() {
    window.open(this.learnMoreUrl);
  },

  /** @private */
  onSearchChanged_(e) {
    if (this.searchTerm === e.detail) {
      return;
    }

    this.searchTerm = e.detail;
    if (this.preserveSearchTerm && this.active_) {
      this.preserveSearchInput_();
    }
  },

  /** @private */
  getBackButtonAriaLabel_() {
    return this.i18n('subpageBackButtonAriaLabel', this.pageTitle);
  },

  /** @private */
  getBackButtonAriaRoleDescription_() {
    return this.i18n('subpageBackButtonAriaRoleDescription', this.pageTitle);
  },

  // Override FindShortcutBehavior methods.
  handleFindShortcut(modalContextOpen) {
    if (modalContextOpen) {
      return false;
    }
    this.$$('cr-search-field').getSearchInput().focus();
    return true;
  },

  // Override FindShortcutBehavior methods.
  searchInputHasFocus() {
    const field = this.$$('cr-search-field');
    return field.getSearchInput() === field.shadowRoot.activeElement;
  },
});
