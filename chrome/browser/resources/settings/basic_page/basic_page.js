// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-basic-page' is the settings page containing the actual settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../appearance_page/appearance_page.js';
import '../privacy_page/privacy_page.js';
import '../safety_check_page/safety_check_page.js';
import '../autofill_page/autofill_page.js';
import '../controls/settings_idle_load.js';
import '../on_startup_page/on_startup_page.js';
import '../people_page/people_page.js';
import '../reset_page/reset_profile_banner.js';
import '../search_page/search_page.js';
import '../settings_page/settings_section.js';
import '../settings_page_css.js';
// <if expr="chromeos or lacros">
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
// </if>

// <if expr="not chromeos and not lacros">
import '../default_browser_page/default_browser_page.js';
// </if>

import {assert} from 'chrome://resources/js/assert.m.js';
import {beforeNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {PageVisibility} from '../page_visibility.js';
// <if expr="chromeos or lacros">
import {PrefsBehavior} from '../prefs/prefs_behavior.js';
// </if>
import {routes} from '../route.js';
import {Route, RouteObserverBehavior, Router} from '../router.js';
import {getSearchManager, SearchResult} from '../search_settings.js';
import {MainPageBehavior} from '../settings_page/main_page_behavior.js';

// <if expr="chromeos or lacros">
const OS_BANNER_INTERACTION_METRIC_NAME =
    'ChromeOS.Settings.OsBannerInteraction';

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
const CrosSettingsOsBannerInteraction = {
  NotShown: 0,
  Shown: 1,
  Clicked: 2,
  Closed: 3,
};
// </if>

Polymer({
  is: 'settings-basic-page',

  _template: html`{__html_template__}`,

  behaviors: [
    MainPageBehavior, RouteObserverBehavior,
    // <if expr="chromeos or lacros">
    PrefsBehavior,
    // </if>
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Dictionary defining page visibility.
     * @type {!PageVisibility}
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
     * True if the basic page should currently display the reset profile banner.
     * @private {boolean}
     */
    showResetProfileBanner_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showResetProfileBanner');
      },
    },

    // <if expr="chromeos or lacros">
    /** @private */
    showOSSettingsBanner_: {
      type: Boolean,
      computed: 'computeShowOSSettingsBanner_(' +
          'prefs.settings.cros.show_os_banner.value, currentRoute_)',
    },
    // </if>

    /** @private {!Route|undefined} */
    currentRoute_: Object,

    /**
     * Used to avoid handling a new toggle while currently toggling.
     * @private
     */
    advancedTogglingInProgress_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  hostAttributes: {
    role: 'main',
  },

  listeners: {
    'subpage-expand': 'onSubpageExpanded_',
  },

  // <if expr="chromeos or lacros">
  /** @private {boolean} */
  osBannerShowMetricRecorded_: false,
  // </if>

  /** @override */
  attached() {
    this.currentRoute_ = Router.getInstance().getCurrentRoute();
  },

  /**
   * @param {!Route} newRoute
   * @param {Route} oldRoute
   */
  currentRouteChanged(newRoute, oldRoute) {
    this.currentRoute_ = newRoute;

    if (routes.ADVANCED && routes.ADVANCED.contains(newRoute)) {
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

    MainPageBehavior.currentRouteChanged.call(this, newRoute, oldRoute);
  },

  // Override MainPageBehavior method.
  containsRoute(route) {
    return !route || routes.BASIC.contains(route) ||
        routes.ADVANCED.contains(route);
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
   * @return {!Promise<!SearchResult>} A signal indicating that
   *     searching finished.
   */
  searchContents(query) {
    const whenSearchDone = [
      getSearchManager().search(query, assert(this.$$('#basicPage'))),
    ];

    if (this.pageVisibility.advancedSettings !== false) {
      whenSearchDone.push(
          this.$$('#advancedPageTemplate').get().then(function(advancedPage) {
            return getSearchManager().search(query, advancedPage);
          }));
    }

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

  // <if expr="chromeos or lacros">
  /**
   * @return {boolean|undefined}
   * @private
   */
  computeShowOSSettingsBanner_() {
    // this.prefs is implicitly used by this.getPref() below.
    if (!this.prefs || !this.currentRoute_) {
      return;
    }
    const showPref = /** @type {boolean} */ (
        this.getPref('settings.cros.show_os_banner').value);

    // Banner only shows on the main page because direct navigations to a
    // sub-page are unlikely to be due to a user looking for an OS setting.
    const show = showPref && !this.currentRoute_.isSubpage();

    // Record the show metric once. We can't record the metric in attached()
    // because prefs might not be ready yet.
    if (!this.osBannerShowMetricRecorded_) {
      chrome.metricsPrivate.recordEnumerationValue(
          OS_BANNER_INTERACTION_METRIC_NAME,
          show ? CrosSettingsOsBannerInteraction.Shown :
                 CrosSettingsOsBannerInteraction.NotShown,
          Object.keys(CrosSettingsOsBannerInteraction).length);
      this.osBannerShowMetricRecorded_ = true;
    }
    return show;
  },

  /** @private */
  onOSSettingsBannerClick_() {
    // The label has a link that opens the page, so just record the metric.
    chrome.metricsPrivate.recordEnumerationValue(
        OS_BANNER_INTERACTION_METRIC_NAME,
        CrosSettingsOsBannerInteraction.Clicked,
        Object.keys(CrosSettingsOsBannerInteraction).length);
  },

  /** @private */
  onOSSettingsBannerClosed_() {
    this.setPrefValue('settings.cros.show_os_banner', false);
    chrome.metricsPrivate.recordEnumerationValue(
        OS_BANNER_INTERACTION_METRIC_NAME,
        CrosSettingsOsBannerInteraction.Closed,
        Object.keys(CrosSettingsOsBannerInteraction).length);
  },
  // </if>

  /** @private */
  onResetProfileBannerClosed_() {
    this.showResetProfileBanner_ = false;
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
    // beforeNextRender() must be used instead.
    beforeNextRender(this, () => {
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
   * @param {!Route} currentRoute
   * @param {boolean} inSearchMode
   * @param {boolean} hasExpandedSection
   * @return {boolean} Whether to show the basic page, taking into account
   *     both routing and search state.
   * @private
   */
  showBasicPage_(currentRoute, inSearchMode, hasExpandedSection) {
    return !hasExpandedSection || routes.BASIC.contains(currentRoute);
  },

  /**
   * @param {!Route} currentRoute
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
        (routes.ADVANCED && routes.ADVANCED.contains(currentRoute)) :
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
