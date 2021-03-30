// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-data' handles showing the local storage summary list for all sites.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../settings_shared_css.js';
import './site_data_entry.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {ListPropertyUpdateBehavior} from 'chrome://resources/js/list_property_update_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GlobalScrollTargetBehavior, GlobalScrollTargetBehaviorImpl} from '../global_scroll_target_behavior.js';
import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Route, RouteObserverBehavior, Router} from '../router.js';

import {LocalDataBrowserProxy, LocalDataBrowserProxyImpl, LocalDataItem} from './local_data_browser_proxy.js';
import {SiteSettingsBehavior} from './site_settings_behavior.js';

/**
 * @typedef {{
 *   id: string,
 *   start: number,
 *   count: number,
 * }}
 */
let CookieRemovePacket;

Polymer({
  is: 'site-data',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    ListPropertyUpdateBehavior,
    GlobalScrollTargetBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * The current filter applied to the cookie data list.
     */
    filter: {
      observer: 'onFilterChanged_',
      notify: true,
      type: String,
    },

    /** @type {!Map<string, (string|Function)>} */
    focusConfig: {
      type: Object,
      observer: 'focusConfigChanged_',
    },

    isLoading_: Boolean,

    /** @type {!Array<!LocalDataItem>} */
    sites: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
     * GlobalScrollTargetBehavior
     * @override
     */
    subpageRoute: {
      type: Object,
      value: routes.SITE_SETTINGS_SITE_DATA,
    },

    /** @private */
    lastFocused_: Object,

    /** @private */
    listBlurred_: Boolean,
  },

  /** @private {LocalDataBrowserProxy} */
  browserProxy_: null,

  /**
   * When navigating to site data details sub-page, |lastSelected_| holds the
   * site name as well as the index of the selected site. This is used when
   * navigating back to site data in order to focus on the correct site.
   * @private {!{item: !LocalDataItem, index: number}|null}
   */
  lastSelected_: null,

  /** @override */
  created() {
    this.browserProxy_ = LocalDataBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'on-tree-item-removed', this.updateSiteList_.bind(this));
  },

  /**
   * Reload cookies when the site data page is visited.
   *
   * RouteObserverBehavior
   * @param {!Route} currentRoute
   * @param {!Route} previousRoute
   * @protected
   */
  currentRouteChanged(currentRoute, previousRoute) {
    GlobalScrollTargetBehaviorImpl.currentRouteChanged.call(this, currentRoute);
    // Reload cookies on navigation to the site data page from a different
    // page. Avoid reloading on repeated navigations to the same page, as these
    // are likely search queries.
    if (currentRoute === routes.SITE_SETTINGS_SITE_DATA &&
        currentRoute !== previousRoute) {
      this.isLoading_ = true;
      // Needed to fix iron-list rendering issue. The list will not render
      // correctly until a scroll occurs.
      // See https://crbug.com/853906.
      const ironList = /** @type {!IronListElement} */ (this.$$('iron-list'));
      ironList.scrollToIndex(0);
      this.browserProxy_.reloadCookies().then(this.updateSiteList_.bind(this));
    }
  },

  /**
   * @param {!Map<string, (string|Function)>} newConfig
   * @param {?Map<string, (string|Function)>} oldConfig
   * @private
   */
  focusConfigChanged_(newConfig, oldConfig) {
    // focusConfig is set only once on the parent, so this observer should only
    // fire once.
    assert(!oldConfig);

    // Populate the |focusConfig| map of the parent <settings-animated-pages>
    // element, with additional entries that correspond to subpage trigger
    // elements residing in this element's Shadow DOM.
    if (routes.SITE_SETTINGS_DATA_DETAILS) {
      const onNavigatedTo = () => this.async(() => {
        if (this.lastSelected_ === null || this.sites.length === 0) {
          return;
        }

        const lastSelectedSite = this.lastSelected_.item.site;
        const lastSelectedIndex = this.lastSelected_.index;
        this.lastSelected_ = null;

        const indexFromId =
            this.sites.findIndex(site => site.site === lastSelectedSite);

        // If the site is no longer in |sites|, use the index as a fallback.
        // Since the sites are sorted, an alternative could be to select the
        // site that comes next in sort order.
        const indexFallback = lastSelectedIndex < this.sites.length ?
            lastSelectedIndex :
            this.sites.length - 1;
        const index = indexFromId > -1 ? indexFromId : indexFallback;
        this.focusOnSiteSelectButton_(index);
      });
      this.focusConfig.set(
          routes.SITE_SETTINGS_DATA_DETAILS.path, onNavigatedTo);
    }
  },

  /**
   * @param {number} index
   * @private
   */
  focusOnSiteSelectButton_(index) {
    const ironList =
        /** @type {!IronListElement} */ (this.$$('iron-list'));
    ironList.focusItem(index);
    const siteToSelect = this.sites[index].site.replace(/[.]/g, '\\.');
    const button = this.$$(`#siteItem_${siteToSelect}`).$$('.subpage-arrow');
    focusWithoutInk(assert(button));
  },

  /**
   * @param {string} current
   * @param {string|undefined} previous
   * @private
   */
  onFilterChanged_(current, previous) {
    // Ignore filter changes which do not occur on the site data page. The
    // site settings data details subpage expects the tree model to remain in
    // the same state.
    if (previous === undefined ||
        Router.getInstance().getCurrentRoute() !==
            routes.SITE_SETTINGS_SITE_DATA) {
      return;
    }
    this.updateSiteList_();
  },

  /**
   * Gather all the site data.
   * @private
   */
  updateSiteList_() {
    this.isLoading_ = true;
    this.browserProxy_.getDisplayList(this.filter).then(localDataItems => {
      this.updateList('sites', item => item.site, localDataItems);
      this.isLoading_ = false;
      this.fire('site-data-list-complete');
    });
  },

  /**
   * Returns the string to use for the Remove label.
   * @param {string} filter The current filter string.
   * @return {string}
   * @private
   */
  computeRemoveLabel_(filter) {
    if (filter.length === 0) {
      return loadTimeData.getString('siteSettingsCookieRemoveAll');
    }
    return loadTimeData.getString('siteSettingsCookieRemoveAllShown');
  },

  /** @private */
  onCloseDialog_() {
    this.$.confirmDeleteDialog.close();
  },

  /** @private */
  onCloseThirdPartyDialog_() {
    this.$.confirmDeleteThirdPartyDialog.close();
  },

  /** @private */
  onConfirmDeleteDialogClosed_() {
    focusWithoutInk(assert(this.$.removeShowingSites));
  },

  /** @private */
  onConfirmDeleteThirdPartyDialogClosed_() {
    focusWithoutInk(assert(this.$.removeAllThirdPartyCookies));
  },

  /**
   * Shows a dialog to confirm the deletion of multiple sites.
   * @param {!Event} e
   * @private
   */
  onRemoveShowingSitesTap_(e) {
    e.preventDefault();
    this.$.confirmDeleteDialog.showModal();
  },

  /**
   * Shows a dialog to confirm the deletion of cookies available
   * in third-party contexts and associated site data.
   * @private
   */
  onRemoveThirdPartyCookiesTap_(e) {
    e.preventDefault();
    this.$.confirmDeleteThirdPartyDialog.showModal();
  },

  /**
   * Called when deletion for all showing sites has been confirmed.
   * @private
   */
  onConfirmDelete_() {
    this.$.confirmDeleteDialog.close();
    if (this.filter.length === 0) {
      MetricsBrowserProxyImpl.getInstance().recordSettingsPageHistogram(
          PrivacyElementInteractions.SITE_DATA_REMOVE_ALL);
      this.browserProxy_.removeAll().then(() => {
        this.sites = [];
      });
    } else {
      MetricsBrowserProxyImpl.getInstance().recordSettingsPageHistogram(
          PrivacyElementInteractions.SITE_DATA_REMOVE_FILTERED);
      this.browserProxy_.removeShownItems();
      // We just deleted all items found by the filter, let's reset the filter.
      this.fire('clear-subpage-search');
    }
  },

  /**
   * Called when deletion of all third-party cookies and site data has been
   * confirmed.
   * @private
   */
  onConfirmThirdPartyDelete_() {
    this.$.confirmDeleteThirdPartyDialog.close();
    this.browserProxy_.removeAllThirdPartyCookies().then(() => {
      this.updateSiteList_();
    });
  },

  /**
   * @param {!{model: !{item: !LocalDataItem, index: number}}} event
   * @private
   */
  onSiteClick_(event) {
    // If any delete button is selected, the focus will be in a bad state when
    // returning to this page. To avoid this, the site select button is given
    // focus. See https://crbug.com/872197.
    this.focusOnSiteSelectButton_(event.model.index);
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_DATA_DETAILS,
        new URLSearchParams('site=' + event.model.item.site));
    this.lastSelected_ = event.model;
  },

  /**
   * @private
   * @return {boolean}
   */
  showRemoveThirdPartyCookies_() {
    return loadTimeData.getBoolean('enableRemovingAllThirdPartyCookies') &&
        this.sites.length > 0 && this.filter.length === 0;
  },
});
