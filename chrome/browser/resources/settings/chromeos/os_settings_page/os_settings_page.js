// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-page' is the settings page containing the actual OS settings.
 */
(function() {
'use strict';

const BROWSER_BANNER_INTERACTION_METRIC_NAME =
    'ChromeOS.Settings.BrowserBannerInteraction';

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
const CrosSettingsBrowserBannerInteraction = {
  NotShown: 0,
  Shown: 1,
  Clicked: 2,
  Closed: 3,
};

Polymer({
  is: 'os-settings-page',

  behaviors: [
    settings.MainPageBehavior,
    settings.RouteObserverBehavior,
    PrefsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    showAndroidApps: Boolean,

    showAppManagement: Boolean,

    showApps: Boolean,

    showCrostini: Boolean,

    showReset: Boolean,

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
     * @type {!PageVisibility}
     */
    pageVisibility: {
      type: Object,
      value: function() {
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

    showBrowserSettingsBanner_: {
      type: Boolean,
      computed: 'computeShowBrowserSettingsBanner_(' +
          'prefs.settings.cros.show_browser_banner.value, currentRoute_)',
    },

    /** @private {!settings.Route|undefined} */
    currentRoute_: Object,
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

  /** @private {boolean} */
  browserBannerShowMetricRecorded_: false,

  /** @override */
  attached: function() {
    this.currentRoute_ = settings.getCurrentRoute();

    this.allowCrostini_ = loadTimeData.valueExists('allowCrostini') &&
        loadTimeData.getBoolean('allowCrostini');

    if (settings.AndroidAppsBrowserProxyImpl) {
      this.addWebUIListener(
          'android-apps-info-update', this.androidAppsInfoUpdate_.bind(this));
      settings.AndroidAppsBrowserProxyImpl.getInstance()
          .requestAndroidAppsInfo();
    }
  },

  /**
   * @param {!settings.Route} newRoute
   * @param {settings.Route} oldRoute
   */
  currentRouteChanged: function(newRoute, oldRoute) {
    this.currentRoute_ = newRoute;

    if (settings.routes.ADVANCED &&
        settings.routes.ADVANCED.contains(newRoute)) {
      this.advancedToggleExpanded = true;
    }

    if (oldRoute && oldRoute.isSubpage()) {
      // If the new route isn't the same expanded section, reset
      // hasExpandedSection_ for the next transition.
      if (!newRoute.isSubpage() || newRoute.section != oldRoute.section) {
        this.hasExpandedSection_ = false;
      }
    } else {
      assert(!this.hasExpandedSection_);
    }

    settings.MainPageBehavior.currentRouteChanged.call(
        this, newRoute, oldRoute);
  },

  // Override settings.MainPageBehavior method.
  containsRoute: function(route) {
    return !route || settings.routes.BASIC.contains(route) ||
        settings.routes.ADVANCED.contains(route);
  },

  /**
   * @param {boolean|undefined} visibility
   * @return {boolean}
   * @private
   */
  showPage_: function(visibility) {
    return visibility !== false;
  },

  /**
   * Queues a task to search the basic sections, then another for the advanced
   * sections.
   * @param {string} query The text to search for.
   * @return {!Promise<!settings.SearchResult>} A signal indicating that
   *     searching finished.
   */
  searchContents: function(query) {
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
  computeShowSecondaryUserBanner_: function() {
    return !this.hasExpandedSection_ &&
        loadTimeData.getBoolean('isSecondaryUser');
  },

  /**
   * @return {boolean|undefined}
   * @private
   */
  computeShowBrowserSettingsBanner_: function() {
    // this.prefs is implicitly used by this.getPref() below, but may not be
    // initialized yet.
    if (!this.prefs || !this.currentRoute_) {
      return;
    }
    const showPref = /** @type {boolean} */ (
        this.getPref('settings.cros.show_browser_banner').value);

    // Banner only shows on the main page because direct navigations to a
    // sub-page (e.g. to the bluetooth section from the system tray) are
    // unlikely to be due to a user looking for a browser setting.
    const show = showPref && !this.currentRoute_.isSubpage();

    // Record the show metric once. We can't record the metric in attached()
    // because prefs might not be ready yet.
    if (!this.browserBannerShowMetricRecorded_) {
      chrome.metricsPrivate.recordEnumerationValue(
          BROWSER_BANNER_INTERACTION_METRIC_NAME,
          show ? CrosSettingsBrowserBannerInteraction.Shown :
                 CrosSettingsBrowserBannerInteraction.NotShown,
          Object.keys(CrosSettingsBrowserBannerInteraction).length);
      this.browserBannerShowMetricRecorded_ = true;
    }
    return show;
  },

  /**
   * @param {!AndroidAppsInfo} info
   * @private
   */
  androidAppsInfoUpdate_: function(info) {
    this.androidAppsInfo = info;
  },

  /**
   * Returns true in case Android apps settings should be shown. It is not
   * shown in case we don't have the Play Store app and settings app is not
   * yet available.
   * @return {boolean}
   * @private
   */
  shouldShowAndroidAppsSection_: function() {
    if (this.havePlayStoreApp ||
        (this.androidAppsInfo && this.androidAppsInfo.settingsAppAvailable)) {
      return true;
    }
    return false;
  },

  /** @private */
  onBrowserSettingsClick_: function() {
    // The label has a link that opens the page, so just record the metric.
    chrome.metricsPrivate.recordEnumerationValue(
        BROWSER_BANNER_INTERACTION_METRIC_NAME,
        CrosSettingsBrowserBannerInteraction.Clicked,
        Object.keys(CrosSettingsBrowserBannerInteraction).length);
  },

  /** @private */
  onBrowserSettingsBannerClosed_: function() {
    this.setPrefValue('settings.cros.show_browser_banner', false);
    chrome.metricsPrivate.recordEnumerationValue(
        BROWSER_BANNER_INTERACTION_METRIC_NAME,
        CrosSettingsBrowserBannerInteraction.Closed,
        Object.keys(CrosSettingsBrowserBannerInteraction).length);
  },

  /**
   * Hides everything but the newly expanded subpage.
   * @private
   */
  onSubpageExpanded_: function() {
    this.hasExpandedSection_ = true;
  },

  /**
   * Render the advanced page now (don't wait for idle).
   * @private
   */
  advancedToggleExpandedChanged_: function() {
    if (!this.advancedToggleExpanded) {
      return;
    }

    // In Polymer2, async() does not wait long enough for layout to complete.
    // Polymer.RenderStatus.beforeNextRender() must be used instead.
    Polymer.RenderStatus.beforeNextRender(this, () => {
      this.$$('#advancedPageTemplate').get();
    });
  },

  advancedToggleClicked_: function() {
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
  showAdvancedToggle_: function(inSearchMode, hasExpandedSection) {
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
  showBasicPage_: function(currentRoute, inSearchMode, hasExpandedSection) {
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
  showAdvancedPage_: function(
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
  showAdvancedSettings_: function(visibility) {
    return visibility !== false;
  },

  /**
   * @param {boolean} opened Whether the menu is expanded.
   * @return {string} Icon name.
   * @private
   */
  getArrowIcon_: function(opened) {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  },

  /**
   * @param {boolean} bool
   * @return {string}
   * @private
   */
  boolToString_: function(bool) {
    return bool.toString();
  },
});
})();
