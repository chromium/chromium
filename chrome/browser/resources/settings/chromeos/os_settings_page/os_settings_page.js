// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-page' is the settings page containing the actual OS settings.
 */

Polymer({
  is: 'os-settings-page',

  behaviors: [
    settings.MainPageBehavior,
    settings.RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    showAndroidApps: Boolean,

    showCrostini: Boolean,

    showPluginVm: Boolean,

    showReset: Boolean,

    showStartup: Boolean,

    showKerberosSection: Boolean,

    allowCrostini_: Boolean,

    havePlayStoreApp: Boolean,

    /** @type {!AndroidAppsInfo|undefined} */
    androidAppsInfo: Object,

    /**
     * Whether the user is in guest mode.
     * @private {boolean}
     */
    isGuestMode_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isGuest'),
    },

    /**
     * Dictionary defining page visibility.
     * @type {!OSPageVisibility}
     */
    pageVisibility: {
      type: Object,
      value() {
        return {};
      },
    },

    advancedToggleExpanded: {
      type: Boolean,
      value: false,
      notify: true,
      observer: 'advancedToggleExpandedChanged_',
    },

    /**
     * True if a section is fully expanded to hide other sections beneath it.
     * False otherwise (even while animating a section open/closed).
     * @private {boolean}
     */
    hasExpandedSection_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the user is a secondary user. Computed so that it is calculated
     * correctly after loadTimeData is available.
     * @private
     */
    showSecondaryUserBanner_: {
      type: Boolean,
      computed: 'computeShowSecondaryUserBanner_(hasExpandedSection_)',
    },

    /**
     * Whether to show banner indicating the user to return this device as an
     * update is required as per policy but the device has reached end of life.
     * @private
     */
    showUpdateRequiredEolBanner_: {
      type: Boolean,
      value: !!loadTimeData.getString('updateRequiredEolBannerText'),
    },

    /** @private {!settings.Route|undefined} */
    currentRoute_: Object,

    /**
     * True if redesign of account management flows is enabled.
     * @private
     */
    isAccountManagementFlowsV2Enabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isAccountManagementFlowsV2Enabled');
      },
      readOnly: true,
    },
  },

  hostAttributes: {
    role: 'main',
  },

  listeners: {
    'subpage-expand': 'onSubpageExpanded_',
  },

  /**
   * Used to avoid handling a new toggle while currently toggling.
   * @private {boolean}
   */
  advancedTogglingInProgress_: false,

  /** @override */
  attached: function() {
    this.currentRoute_ = settings.Router.getInstance().getCurrentRoute();

    this.allowCrostini_ = loadTimeData.valueExists('allowCrostini') &&
        loadTimeData.getBoolean('allowCrostini');

    this.addWebUIListener(
        'android-apps-info-update', this.androidAppsInfoUpdate_.bind(this));
    settings.AndroidAppsBrowserProxyImpl.getInstance().requestAndroidAppsInfo();
  },

  /**
   * @param {!settings.Route} newRoute
   * @param {settings.Route} oldRoute
   */
  currentRouteChanged(newRoute, oldRoute) {
    this.currentRoute_ = newRoute;

    if (settings.routes.ADVANCED &&
        settings.routes.ADVANCED.contains(newRoute)) {
      this.advancedToggleExpanded = true;
    }

    if (oldRoute && oldRoute.isSubpage()) {
      // If the new route isn't the same expanded section, reset
      // hasExpandedSection_ for the next transition.
      if (!newRoute.isSubpage() || newRoute.section !== oldRoute.section) {
        this.hasExpandedSection_ = false;
      }
    } else {
      assert(!this.hasExpandedSection_);
    }

    settings.MainPageBehavior.currentRouteChanged.call(
        this, newRoute, oldRoute);
  },

  // Override settings.MainPageBehavior method.
  containsRoute(route) {
    return !route || settings.routes.BASIC.contains(route) ||
        settings.routes.ADVANCED.contains(route);
  },

  /**
   * @param {boolean|undefined} visibility
   * @return {boolean}
   * @private
   */
  showPage_(visibility) {
    return visibility !== false;
  },

  /**
   * Queues a task to search the basic sections, then another for the advanced
   * sections.
   * @param {string} query The text to search for.
   * @return {!Promise<!settings.SearchResult>} A signal indicating that
   *     searching finished.
   */
  searchContents(query) {
    const whenSearchDone = [
      settings.getSearchManager().search(query, assert(this.$$('#basicPage'))),
    ];

    whenSearchDone.push(
        this.$$('#advancedPageTemplate').get().then(function(advancedPage) {
          return settings.getSearchManager().search(query, advancedPage);
        }));

    return Promise.all(whenSearchDone).then(function(requests) {
      // Combine the SearchRequests results to a single SearchResult object.
      return {
        canceled: requests.some(function(r) {
          return r.canceled;
        }),
        didFindMatches: requests.some(function(r) {
          return r.didFindMatches();
        }),
        // All requests correspond to the same user query, so only need to check
        // one of them.
        wasClearSearch: requests[0].isSame(''),
      };
    });
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowSecondaryUserBanner_() {
    return !this.hasExpandedSection_ &&
        loadTimeData.getBoolean('isSecondaryUser');
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowUpdateRequiredEolBanner_() {
    return !this.hasExpandedSection_ && this.showUpdateRequiredEolBanner_;
  },

  /**
   * @param {!AndroidAppsInfo} info
   * @private
   */
  androidAppsInfoUpdate_(info) {
    this.androidAppsInfo = info;
  },

  /**
   * Hides the update required EOL banner. It is shown again when Settings is
   * re-opened.
   * @private
   */
  onCloseEolBannerClicked_() {
    this.showUpdateRequiredEolBanner_ = false;
  },

  /**
   * Hides everything but the newly expanded subpage.
   * @private
   */
  onSubpageExpanded_() {
    this.hasExpandedSection_ = true;
  },

  /**
   * Render the advanced page now (don't wait for idle).
   * @private
   */
  advancedToggleExpandedChanged_() {
    if (!this.advancedToggleExpanded) {
      return;
    }

    // In Polymer2, async() does not wait long enough for layout to complete.
    // Polymer.RenderStatus.beforeNextRender() must be used instead.
    Polymer.RenderStatus.beforeNextRender(this, () => {
      this.$$('#advancedPageTemplate').get();
    });
  },

  advancedToggleClicked_() {
    if (this.advancedTogglingInProgress_) {
      return;
    }

    this.advancedTogglingInProgress_ = true;
    const toggle = this.$$('#toggleContainer');
    if (!this.advancedToggleExpanded) {
      this.advancedToggleExpanded = true;
      this.async(() => {
        this.$$('#advancedPageTemplate').get().then(() => {
          this.fire('scroll-to-top', {
            top: toggle.offsetTop,
            callback: () => {
              this.advancedTogglingInProgress_ = false;
            }
          });
        });
      });
    } else {
      this.fire('scroll-to-bottom', {
        bottom: toggle.offsetTop + toggle.offsetHeight + 24,
        callback: () => {
          this.advancedToggleExpanded = false;
          this.advancedTogglingInProgress_ = false;
        }
      });
    }
  },

  /**
   * @param {boolean} inSearchMode
   * @param {boolean} hasExpandedSection
   * @return {boolean}
   * @private
   */
  showAdvancedToggle_(inSearchMode, hasExpandedSection) {
    return !inSearchMode && !hasExpandedSection;
  },

  /**
   * @param {!settings.Route} currentRoute
   * @param {boolean} inSearchMode
   * @param {boolean} hasExpandedSection
   * @return {boolean} Whether to show the basic page, taking into account
   *     both routing and search state.
   * @private
   */
  showBasicPage_(currentRoute, inSearchMode, hasExpandedSection) {
    return !hasExpandedSection || settings.routes.BASIC.contains(currentRoute);
  },

  /**
   * @param {!settings.Route} currentRoute
   * @param {boolean} inSearchMode
   * @param {boolean} hasExpandedSection
   * @param {boolean} advancedToggleExpanded
   * @return {boolean} Whether to show the advanced page, taking into account
   *     both routing and search state.
   * @private
   */
  showAdvancedPage_(
      currentRoute, inSearchMode, hasExpandedSection, advancedToggleExpanded) {
    return hasExpandedSection ?
        (settings.routes.ADVANCED &&
         settings.routes.ADVANCED.contains(currentRoute)) :
        advancedToggleExpanded || inSearchMode;
  },

  /**
   * @param {(boolean|undefined)} visibility
   * @return {boolean} True unless visibility is false.
   * @private
   */
  showAdvancedSettings_(visibility) {
    return visibility !== false;
  },

  /**
   * @param {boolean} opened Whether the menu is expanded.
   * @return {string} Icon name.
   * @private
   */
  getArrowIcon_(opened) {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  },

  /**
   * @param {boolean} bool
   * @return {string}
   * @private
   */
  boolToString_(bool) {
    return bool.toString();
  },
});
