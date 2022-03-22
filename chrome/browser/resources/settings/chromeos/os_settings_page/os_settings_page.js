// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-page' is the settings page containing the actual OS settings.
 */

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/hidden_style_css.m.js';
import '//resources/cr_elements/icons.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './settings_idle_load.js';
import '../os_apps_page/os_apps_page.js';
import '../os_people_page/os_people_page.js';
import '../os_privacy_page/os_privacy_page.js';
import '../os_printing_page/os_printing_page.js';
import '../os_search_page/os_search_page.js';
import '../personalization_page/personalization_page.js';
import '../../settings_page/settings_section.js';
import '../../settings_page_css.js';
import '../bluetooth_page/bluetooth_page.js';
import '../device_page/device_page.js';
import '../internet_page/internet_page.js';
import '../kerberos_page/kerberos_page.js';
import '../multidevice_page/multidevice_page.js';
import '../os_bluetooth_page/os_bluetooth_page.js';
import '../os_icons.js';

import {assert} from '//resources/js/assert.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {beforeNextRender, html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {AndroidAppsBrowserProxyImpl, AndroidAppsInfo} from '../os_apps_page/android_apps_browser_proxy.js';
import {OSPageVisibility, osPageVisibility} from '../os_page_visibility.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {MainPageBehavior} from './main_page_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-page',

  behaviors: [
    MainPageBehavior,
    RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    showAndroidApps: Boolean,

    showArcvmManageUsb: Boolean,

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
     * Whether Accessibility OS Settings visibility improvements are enabled.
     * @private{boolean}
     */
    isAccessibilityOSSettingsVisibilityEnabled_: {
      type: Boolean,
      readOnly: true,
      value() {
        return loadTimeData.getBoolean(
            'isAccessibilityOSSettingsVisibilityEnabled');
      }
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

    /** @private {!Route|undefined} */
    currentRoute_: Object,

    /** @private */
    isBluetoothRevampEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableBluetoothRevamp');
      }
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
    this.currentRoute_ = Router.getInstance().getCurrentRoute();

    this.allowCrostini_ = loadTimeData.valueExists('allowCrostini') &&
        loadTimeData.getBoolean('allowCrostini');

    this.addWebUIListener(
        'android-apps-info-update', this.androidAppsInfoUpdate_.bind(this));
    AndroidAppsBrowserProxyImpl.getInstance().requestAndroidAppsInfo();
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
   * @param {!Route} currentRoute
   * @param {boolean} hasExpandedSection
   * @return {boolean} Whether to show the basic page, taking into account
   *     both routing and search state.
   * @private
   */
  showBasicPage_(currentRoute, hasExpandedSection) {
    return !hasExpandedSection || routes.BASIC.contains(currentRoute);
  },

  /**
   * @param {!Route} currentRoute
   * @param {boolean} hasExpandedSection
   * @param {boolean} advancedToggleExpanded
   * @return {boolean} Whether to show the advanced page, taking into account
   *     both routing and search state.
   * @private
   */
  showAdvancedPage_(currentRoute, hasExpandedSection, advancedToggleExpanded) {
    return hasExpandedSection ?
        (routes.ADVANCED && routes.ADVANCED.contains(currentRoute)) :
        advancedToggleExpanded;
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
