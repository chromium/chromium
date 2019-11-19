// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{about: boolean, settings: boolean}}
 */
let MainPageVisibility;

/**
 * @fileoverview
 * 'os-settings-main' displays the selected settings page.
 */
Polymer({
  is: 'os-settings-main',

  behaviors: [settings.RouteObserverBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    advancedToggleExpanded: {
      type: Boolean,
      notify: true,
    },

    /** @private */
    overscroll_: {
      type: Number,
      observer: 'overscrollChanged_',
    },

    /**
     * Controls which main pages are displayed via dom-ifs, based on the current
     * route.
     * @private {!MainPageVisibility}
     */
    showPages_: {
      type: Object,
      value: function() {
        return {about: false, settings: false};
      },
    },

    /**
     * Whether a search operation is in progress or previous search results are
     * being displayed.
     * @private {boolean}
     */
    inSearchMode_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showNoResultsFound_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showingSubpage_: Boolean,

    toolbarSpinnerActive: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /**
     * Dictionary defining page visibility.
     * @type {!PageVisibility}
     */
    pageVisibility: Object,

    showAndroidApps: Boolean,

    showAppManagement: Boolean,

    showApps: Boolean,

    showCrostini: Boolean,

    showReset: Boolean,

    havePlayStoreApp: Boolean,
  },

  /** @private */
  overscrollChanged_: function() {
    if (!this.overscroll_ && this.boundScroll_) {
      this.offsetParent.removeEventListener('scroll', this.boundScroll_);
      window.removeEventListener('resize', this.boundScroll_);
      this.boundScroll_ = null;
    } else if (this.overscroll_ && !this.boundScroll_) {
      this.boundScroll_ = () => {
        if (!this.showingSubpage_) {
          this.setOverscroll_(0);
        }
      };
      this.offsetParent.addEventListener('scroll', this.boundScroll_);
      window.addEventListener('resize', this.boundScroll_);
    }
  },

  /**
   * Sets the overscroll padding. Never forces a scroll, i.e., always leaves
   * any currently visible overflow as-is.
   * @param {number=} opt_minHeight The minimum overscroll height needed.
   * @private
   */
  setOverscroll_: function(opt_minHeight) {
    const scroller = this.offsetParent;
    if (!scroller) {
      return;
    }
    const overscroll = this.$.overscroll;
    const visibleBottom = scroller.scrollTop + scroller.clientHeight;
    const overscrollBottom = overscroll.offsetTop + overscroll.scrollHeight;
    // How much of the overscroll is visible (may be negative).
    const visibleOverscroll =
        overscroll.scrollHeight - (overscrollBottom - visibleBottom);
    this.overscroll_ =
        Math.max(opt_minHeight || 0, Math.ceil(visibleOverscroll));
  },

  /**
   * Updates the hidden state of the about and settings pages based on the
   * current route.
   * @param {!settings.Route} newRoute
   */
  currentRouteChanged: function(newRoute) {
    const inAbout = settings.routes.ABOUT.contains(settings.getCurrentRoute());
    this.showPages_ = {about: inAbout, settings: !inAbout};

    if (!newRoute.isSubpage()) {
      document.title = inAbout ? loadTimeData.getStringF(
                                     'settingsAltPageTitle',
                                     loadTimeData.getString('aboutPageTitle')) :
                                 loadTimeData.getString('settings');
    }
  },

  /** @private */
  onShowingSubpage_: function() {
    this.showingSubpage_ = true;
  },

  /** @private */
  onShowingMainPage_: function() {
    this.showingSubpage_ = false;
  },

  /**
   * A handler for the 'showing-section' event fired from os-settings-page,
   * indicating that a section should be scrolled into view as a result of a
   * navigation.
   * @param {!CustomEvent<!HTMLElement>} e
   * @private
   */
  onShowingSection_: function(e) {
    const section = e.detail;
    // Calculate the height that the overscroll padding should be set to, so
    // that the given section is displayed at the top of the viewport.
    // Find the distance from the section's top to the overscroll.
    const sectionTop = section.offsetParent.offsetTop + section.offsetTop;
    const distance = this.$.overscroll.offsetTop - sectionTop;
    const overscroll = Math.max(0, this.offsetParent.clientHeight - distance);
    this.setOverscroll_(overscroll);
    section.scrollIntoView();
    section.focus();
  },

  /**
   * Returns the root page (if it exists) for a route.
   * @param {!settings.Route} route
   * @return {?OsSettingsPageElement}
   */
  getPage_: function(route) {
    if (settings.routes.BASIC.contains(route) ||
        (settings.routes.ADVANCED &&
         settings.routes.ADVANCED.contains(route))) {
      return /** @type {?OsSettingsPageElement} */ (
          this.$$('os-settings-page'));
    }
    assertNotReached();
  },

  /**
   * @param {string} query
   * @return {!Promise} A promise indicating that searching finished.
   */
  searchContents: function(query) {
    // Trigger rendering of the basic and advanced pages and search once ready.
    this.inSearchMode_ = true;
    this.toolbarSpinnerActive = true;

    return new Promise((resolve, reject) => {
      setTimeout(() => {
        const whenSearchDone =
            assert(this.getPage_(settings.routes.BASIC)).searchContents(query);
        whenSearchDone.then(result => {
          resolve();
          if (result.canceled) {
            // Nothing to do here. A previous search request was canceled
            // because a new search request was issued with a different query
            // before the previous completed.
            return;
          }

          this.toolbarSpinnerActive = false;
          this.inSearchMode_ = !result.wasClearSearch;
          this.showNoResultsFound_ =
              this.inSearchMode_ && !result.didFindMatches;

          if (this.inSearchMode_) {
            Polymer.IronA11yAnnouncer.requestAvailability();
            this.fire('iron-announce', {
              text: this.showNoResultsFound_ ?
                  loadTimeData.getString('searchNoResults') :
                  loadTimeData.getStringF('searchResults', query)
            });
          }
        });
      }, 0);
    });
  },

  /**
   * @return {boolean}
   * @private
   */
  showManagedHeader_: function() {
    return !this.inSearchMode_ && !this.showingSubpage_ &&
        !this.showPages_.about;
  },
});
