// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'all-sites' is the polymer element for showing the list of all sites under
 * Site Settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../settings_shared_css.js';
import './all_sites_icons.js';
import './clear_storage_dialog_css.js';
import './site_entry.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GlobalScrollTargetBehavior, GlobalScrollTargetBehaviorImpl} from '../global_scroll_target_behavior.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Route, RouteObserverBehavior, Router} from '../router.js';

import {ALL_SITES_DIALOG, AllSitesAction2, ContentSetting, ContentSettingsTypes, SortMethod} from './constants.js';
import {LocalDataBrowserProxy, LocalDataBrowserProxyImpl} from './local_data_browser_proxy.js';
import {SiteSettingsBehavior} from './site_settings_behavior.js';
import {SiteGroup} from './site_settings_prefs_browser_proxy.js';

Polymer({
  is: 'all-sites',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    SiteSettingsBehavior,
    WebUIListenerBehavior,
    RouteObserverBehavior,
    GlobalScrollTargetBehavior,
  ],

  properties: {
    // TODO(https://crbug.com/1037809): Refactor siteGroupMap to use an Object
    // instead of a Map so that it's observable by Polymore more naturally. As
    // it stands, one cannot use computed properties based off the value of
    // siteGroupMap nor can one use observable functions to listen to changes
    // to siteGroupMap.
    /**
     * Map containing sites to display in the widget, grouped into their
     * eTLD+1 names.
     * @type {!Map<string, !SiteGroup>}
     */
    siteGroupMap: {
      type: Object,
      value() {
        return new Map();
      },
    },

    /**
     * Filtered site group list.
     * @type {!Array<SiteGroup>}
     * @private
     */
    filteredList_: {
      type: Array,
    },

    /**
     * Needed by GlobalScrollTargetBehavior.
     * @override
     */
    subpageRoute: {
      type: Object,
      value: routes.SITE_SETTINGS_ALL,
      readOnly: true,
    },

    /**
     * The search query entered into the All Sites search textbox. Used to
     * filter the All Sites list.
     * @private
     */
    filter: {
      type: String,
      value: '',
      observer: 'forceListUpdate_',
    },

    /**
     * All possible sort methods.
     * @type {!{name: string, mostVisited: string, storage: string}}
     * @private
     */
    sortMethods_: {
      type: Object,
      value: SortMethod,
      readOnly: true,
    },

    /**
     * Stores the last selected item in the All Sites list.
     * @type {?{item: !SiteGroup, index: number}}
     * @private
     */
    selectedItem_: Object,

    /**
     * @private
     * Used to track the last-focused element across rows for the
     * focusRowBehavior.
     */
    lastFocused_: Object,

    /**
     * @private
     * Used to track whether the list of row items has been blurred for the
     * focusRowBehavior.
     */
    listBlurred_: Boolean,

    /**
     * @private {?{
     *   actionScope: string,
     *   index: number,
     *   item: !SiteGroup,
     *   origin: string,
     *   path: string,
     *   target: !HTMLElement
     * }}
     */
    actionMenuModel_: Object,

    /**
     * @private
     * Used to determine if user is attempting to clear all site data
     * rather than a single site or origin's data.
     */
    clearAllData_: Boolean,

    /**
     * The selected sort method.
     * @type {!SortMethod|undefined}
     * @private
     */
    sortMethod_: String,

    /**
     * The total usage of all sites for this profile.
     * @type {string}
     * @private
     */
    totalUsage_: {
      type: String,
      value: '0 B',
    },
  },

  /** @private {?LocalDataBrowserProxy} */
  localDataBrowserProxy_: null,

  /** @override */
  created() {
    this.localDataBrowserProxy_ = LocalDataBrowserProxyImpl.getInstance();
  },

  listeners: {
    'open-menu': 'onOpenMenu_',
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'onStorageListFetched', this.onStorageListFetched.bind(this));
    this.addEventListener('site-entry-selected', e => {
      const event =
          /** @type {!CustomEvent<!{item: !SiteGroup, index: number}>} */ (e);
      this.selectedItem_ = event.detail;
    });

    const sortParam = Router.getInstance().getQueryParameters().get('sort');
    if (Object.values(this.sortMethods_).includes(sortParam)) {
      this.$.sortMethod.value = sortParam;
    }
    this.sortMethod_ = this.$.sortMethod.value;
  },

  /** @override */
  attached() {
    // Set scrollOffset so the iron-list scrolling accounts for the space the
    // title takes.
    afterNextRender(this, () => {
      this.$.allSitesList.scrollOffset = this.$.allSitesList.offsetTop;
    });
  },

  /**
   * Reload the site list when the all sites page is visited.
   *
   * RouteObserverBehavior
   * @param {!Route} currentRoute
   * @protected
   */
  currentRouteChanged(currentRoute) {
    GlobalScrollTargetBehaviorImpl.currentRouteChanged.call(this, currentRoute);
    if (currentRoute === routes.SITE_SETTINGS_ALL) {
      this.populateList_();
    }
  },

  /**
   * Retrieves a list of all known sites with site details.
   * @private
   */
  populateList_() {
    /** @type {!Array<ContentSettingsTypes>} */
    const contentTypes = this.getCategoryList();
    // Make sure to include cookies, because All Sites handles data storage +
    // cookies as well as regular ContentSettingsTypes.
    if (!contentTypes.includes(ContentSettingsTypes.COOKIES)) {
      contentTypes.push(ContentSettingsTypes.COOKIES);
    }

    this.browserProxy.getAllSites(contentTypes).then((response) => {
      // Create a new map to make an observable change.
      const newMap = /** @type {!Map<string, !SiteGroup>} */
          (new Map(this.siteGroupMap));
      response.forEach(siteGroup => {
        newMap.set(siteGroup.etldPlus1, siteGroup);
      });
      this.siteGroupMap = newMap;
      this.updateTotalUsage_();
      this.forceListUpdate_();
    });
  },

  /**
   * Integrate sites using storage into the existing sites map, as there
   * may be overlap between the existing sites.
   * @param {!Array<!SiteGroup>} list The list of sites using storage.
   */
  onStorageListFetched(list) {
    // Create a new map to make an observable change.
    const newMap = /** @type {!Map<string, !SiteGroup>} */
        (new Map(this.siteGroupMap));
    list.forEach(storageSiteGroup => {
      newMap.set(storageSiteGroup.etldPlus1, storageSiteGroup);
    });
    this.siteGroupMap = newMap;
    this.updateTotalUsage_();
    this.forceListUpdate_();
    this.focusOnLastSelectedEntry_();
  },

  /**
   * Update the total usage by all sites for this profile after updates
   * to the list
   * @private
   */
  updateTotalUsage_() {
    let usageSum = 0;
    for (const [etldPlus1, siteGroup] of this.siteGroupMap) {
      siteGroup.origins.forEach(origin => {
        usageSum += origin.usage;
      });
    }
    this.browserProxy.getFormattedBytes(usageSum).then(totalUsage => {
      this.totalUsage_ = totalUsage;
    });
  },

  /**
   * Filters the all sites list with the given search query text.
   * @param {!Map<string, !SiteGroup>} siteGroupMap The map of sites to filter.
   * @param {string} searchQuery The filter text.
   * @return {!Array<!SiteGroup>}
   * @private
   */
  filterPopulatedList_(siteGroupMap, searchQuery) {
    const result = [];
    for (const [etldPlus1, siteGroup] of siteGroupMap) {
      if (siteGroup.origins.find(
              originInfo => originInfo.origin.includes(searchQuery))) {
        result.push(siteGroup);
      }
    }
    return this.sortSiteGroupList_(result);
  },

  /**
   * Sorts the given SiteGroup list with the currently selected sort method.
   * @param {!Array<!SiteGroup>} siteGroupList The list of sites to sort.
   * @return {!Array<!SiteGroup>}
   * @private
   */
  sortSiteGroupList_(siteGroupList) {
    const sortMethod = this.$.sortMethod.value;
    if (!this.sortMethods_) {
      return siteGroupList;
    }

    if (sortMethod === SortMethod.MOST_VISITED) {
      siteGroupList.sort(this.mostVisitedComparator_);
    } else if (sortMethod === SortMethod.STORAGE) {
      siteGroupList.sort(this.storageComparator_);
    } else if (sortMethod === SortMethod.NAME) {
      siteGroupList.sort(this.nameComparator_);
    }
    return siteGroupList;
  },

  /**
   * Comparator used to sort SiteGroups by the amount of engagement the user has
   * with the origins listed inside it. Note only the maximum engagement is used
   * for each SiteGroup (as opposed to the sum) in order to prevent domains with
   * higher numbers of origins from always floating to the top of the list.
   * @param {!SiteGroup} siteGroup1
   * @param {!SiteGroup} siteGroup2
   * @private
   */
  mostVisitedComparator_(siteGroup1, siteGroup2) {
    const getMaxEngagement = (max, originInfo) => {
      return (max > originInfo.engagement) ? max : originInfo.engagement;
    };
    const score1 = siteGroup1.origins.reduce(getMaxEngagement, 0);
    const score2 = siteGroup2.origins.reduce(getMaxEngagement, 0);
    return score2 - score1;
  },

  /**
   * Comparator used to sort SiteGroups by the amount of storage they use. Note
   * this sorts in descending order.
   * @param {!SiteGroup} siteGroup1
   * @param {!SiteGroup} siteGroup2
   * @private
   */
  storageComparator_(siteGroup1, siteGroup2) {
    const getOverallUsage = siteGroup => {
      let usage = 0;
      siteGroup.origins.forEach(originInfo => {
        usage += originInfo.usage;
      });
      return usage;
    };

    const siteGroup1Size = getOverallUsage(siteGroup1);
    const siteGroup2Size = getOverallUsage(siteGroup2);
    // Use the number of cookies as a tie breaker.
    return siteGroup2Size - siteGroup1Size ||
        siteGroup2.numCookies - siteGroup1.numCookies;
  },

  /**
   * Comparator used to sort SiteGroups by their eTLD+1 name (domain).
   * @param {!SiteGroup} siteGroup1
   * @param {!SiteGroup} siteGroup2
   * @private
   */
  nameComparator_(siteGroup1, siteGroup2) {
    return siteGroup1.etldPlus1.localeCompare(siteGroup2.etldPlus1);
  },

  /**
   * Called when the user chooses a different sort method to the default.
   * @private
   */
  onSortMethodChanged_() {
    this.sortMethod_ = this.$.sortMethod.value;
    this.filteredList_ = this.sortSiteGroupList_(this.filteredList_);
    // Force the iron-list to rerender its items, as the order has changed.
    this.$.allSitesList.fire('iron-resize');
  },

  /**
   * Forces the all sites list to update its list of items, taking into account
   * the search query and the sort method, then re-renders it.
   * @private
   */
  forceListUpdate_() {
    this.filteredList_ =
        this.filterPopulatedList_(this.siteGroupMap, this.filter);
    this.$.allSitesList.fire('iron-resize');
  },

  /**
   * Whether the |siteGroupMap| is empty.
   * @return {boolean}
   * @private
   */
  siteGroupMapEmpty_() {
    return !this.siteGroupMap.size;
  },

  /**
   * Whether the |filteredList_| is empty due to searching.
   * @return {boolean}
   * @private
   */
  noSearchResultFound_() {
    return !this.filteredList_.length && !this.siteGroupMapEmpty_();
  },

  /**
   * Focus on previously selected entry.
   * @private
   */
  focusOnLastSelectedEntry_() {
    if (!this.selectedItem_ || this.siteGroupMap.size === 0) {
      return;
    }
    // Focus the site-entry to ensure the iron-list renders it, otherwise
    // the query selector will not be able to find it. Note the index is
    // used here instead of the item, in case the item was already removed.
    const index =
        Math.max(0, Math.min(this.selectedItem_.index, this.siteGroupMap.size));
    this.$.allSitesList.focusItem(index);
    this.selectedItem_ = null;
  },

  /**
   * Open the overflow menu and ensure that the item is visible in the scroll
   * pane when its menu is opened (it is possible to open off-screen items using
   * keyboard shortcuts).
   * @param {!CustomEvent<{
   *    actionScope: string, index: number, item: !SiteGroup, origin: string,
   *    path: string, target: !HTMLElement
   *    }>} e
   * @private
   */
  onOpenMenu_(e) {
    const index = e.detail.index;
    const list = /** @type {IronListElement} */ (this.$['allSitesList']);
    if (index < list.firstVisibleIndex || index > list.lastVisibleIndex) {
      list.scrollToIndex(index);
    }
    const target = e.detail.target;
    this.actionMenuModel_ = e.detail;
    const menu = /** @type {CrActionMenuElement} */ (this.$.menu.get());
    menu.showAt(target);
  },

  /**
   * Confirms the resetting of all content settings for an origin.
   * @param {!Event} e
   * @private
   */
  onConfirmResetSettings_(e) {
    e.preventDefault();
    const scope =
        this.actionMenuModel_.actionScope === 'origin' ? 'Origin' : 'SiteGroup';
    const scopes = [ALL_SITES_DIALOG.RESET_PERMISSIONS, scope, 'DialogOpened'];
    this.recordUserAction_(scopes);
    this.$.confirmResetSettings.get().showModal();
  },

  /**
   * Confirms the clearing of all storage data for an etld+1.
   * @param {!Event} e
   * @private
   */
  onConfirmClearData_(e) {
    e.preventDefault();
    const {actionScope, index, origin} = this.actionMenuModel_;
    const {origins, hasInstalledPWA} = this.filteredList_[index];

    const scope = actionScope === 'origin' ? 'Origin' : 'SiteGroup';
    const appInstalled = actionScope === 'origin' ?
        (origins.find(o => o.origin === origin) || {}).isInstalled :
        hasInstalledPWA;
    const installed = appInstalled ? 'Installed' : '';

    const scopes =
        [ALL_SITES_DIALOG.CLEAR_DATA, scope, installed, 'DialogOpened'];
    this.recordUserAction_(scopes);
    this.$.confirmClearDataNew.get().showModal();
  },

  /**
   * Confirms the clearing of all storage data for all sites.
   * @param {!Event} e
   * @private
   */
  onConfirmClearAllData_(e) {
    e.preventDefault();
    this.clearAllData_ = true;
    const anyAppsInstalled = this.filteredList_.some(g => g.hasInstalledPWA);
    const scopes = [ALL_SITES_DIALOG.CLEAR_DATA, 'All'];
    const installed = anyAppsInstalled ? 'Installed' : '';
    this.recordUserAction_([...scopes, installed, 'DialogOpened']);
    this.$.confirmClearAllData.get().showModal();
  },

  /** @private */
  onCloseDialog_(e) {
    chrome.metricsPrivate.recordUserAction('AllSites_DialogClosed');
    e.target.closest('cr-dialog').close();
    this.actionMenuModel_ = null;
    this.$.menu.get().close();
  },

  /**
   * Get the appropriate label string for the clear data dialog based on whether
   * user is clearing data for an origin or siteGroup, and whether or not the
   * origin/siteGroup has an associated installed app.
   * @return {string}
   * @private
   */
  getClearDataLabel_: function() {
    // actionMenuModel_ will be null when dialog closes
    if (this.actionMenuModel_ === null) {
      return '';
    }

    const {index, origin} = this.actionMenuModel_;

    const {origins, hasInstalledPWA} = this.filteredList_[index];

    if (origin) {
      const {isInstalled = false} =
          origins.find(o => o.origin === origin) || {};
      const messageId = isInstalled ?
          'siteSettingsOriginDeleteConfirmationInstalled' :
          'siteSettingsOriginDeleteConfirmation';
      return loadTimeData.substituteString(
          this.i18n(messageId), this.originRepresentation(origin));
    } else {
      // Clear SiteGroup
      let messageId;
      if (hasInstalledPWA) {
        const multipleAppsInstalled = (this.filteredList_[index].origins || [])
                                          .filter(o => o.isInstalled)
                                          .length > 1;

        messageId = multipleAppsInstalled ?
            'siteSettingsSiteGroupDeleteConfirmationInstalledPlural' :
            'siteSettingsSiteGroupDeleteConfirmationInstalled';
      } else {
        messageId = 'siteSettingsSiteGroupDeleteConfirmationNew';
      }
      const displayName = this.actionMenuModel_.item.etldPlus1 ||
          this.originRepresentation(
              this.actionMenuModel_.item.origins[0].origin);
      return loadTimeData.substituteString(this.i18n(messageId), displayName);
      }
  },

  /**
   * Get the appropriate label for the reset permissions confirmation
   * dialog, dependent on whether user is resetting permissions for an
   * origin or an entire SiteGroup.
   * @return {string}
   * @private
   */
  getResetPermissionsLabel_: function() {
    if (this.actionMenuModel_ === null) {
      return '';
    }

    if (this.actionMenuModel_.actionScope === 'origin') {
      return loadTimeData.substituteString(
          this.i18n('siteSettingsSiteResetConfirmation'),
          this.originRepresentation(this.actionMenuModel_.origin));
    }
    return loadTimeData.substituteString(
        this.i18n('siteSettingsSiteGroupResetConfirmation'),
        this.actionMenuModel_.item.etldPlus1 ||
            this.originRepresentation(
                this.actionMenuModel_.item.origins[0].origin));
  },
  /**
   * Get the appropriate label for the clear all data confirmation
   * dialog, depending on whether or not any apps are installed.
   * @return {string}
   * @private
   */
  getClearAllDataLabel_: function() {
    const anyAppsInstalled = this.filteredList_.some(g => g.hasInstalledPWA);
    const messageId = anyAppsInstalled ?
        'siteSettingsClearAllStorageConfirmationInstalled' :
        'siteSettingsClearAllStorageConfirmation';
    return loadTimeData.substituteString(
        this.i18n(messageId), this.totalUsage_);
  },

  /**
   * Get the appropriate label for the clear data confirmation
   * dialog, depending on whether the user is clearing data for a
   * single origin or an entire site group.
   * @return {string}
   * @private
   */
  getLogoutLabel_: function() {
    return this.actionMenuModel_.actionScope === 'origin' ?
        this.i18n('siteSettingsSiteClearStorageSignOut') :
        this.i18n('siteSettingsSiteGroupDeleteSignOut');
  },

  /**
   * @param {!Array<string>} scopes
   * @private
   */
  recordUserAction_: function(scopes) {
    chrome.metricsPrivate.recordUserAction(
        ['AllSites', ...scopes].filter(Boolean).join('_'));
  },

  /**
   * Resets permission settings for a single origin.
   * @param {string} origin
   * @private
   */
  resetPermissionsForOrigin_: function(origin) {
    const contentSettingsTypes = this.getCategoryList();
    this.browserProxy.setOriginPermissions(
        origin, contentSettingsTypes, ContentSetting.DEFAULT);
  },

  /**
   * Resets all permissions for a single origin or all origins listed in
   * |siteGroup.origins|.
   * @param {!Event} e
   * @private
   */
  onResetSettings_: function(e) {
    const {actionScope, index, origin} = this.actionMenuModel_;
    const siteGroupToUpdate = this.filteredList_[index];

    const updatedSiteGroup = {
      etldPlus1: siteGroupToUpdate.etldPlus1,
      numCookies: siteGroupToUpdate.numCookies,
      origins: []
    };

    if (actionScope === 'origin') {
      this.browserProxy.recordAction(AllSitesAction2.RESET_ORIGIN_PERMISSIONS);
      this.recordUserAction_(
          [ALL_SITES_DIALOG.RESET_PERMISSIONS, 'Origin', 'Confirm']);

      this.resetPermissionsForOrigin_(origin);
      updatedSiteGroup.origins = siteGroupToUpdate.origins;
      const updatedOrigin =
          updatedSiteGroup.origins.find(o => o.origin === origin);
      updatedOrigin.hasPermissionSettings = false;
      if (updatedOrigin.numCookies <= 0 || updatedOrigin.usage <= 0) {
        updatedSiteGroup.origins =
            updatedSiteGroup.origins.filter(o => o.origin !== origin);
      }
    } else {
      // Reset permissions for entire site group
      this.browserProxy.recordAction(
          AllSitesAction2.RESET_SITE_GROUP_PERMISSIONS);
      this.recordUserAction_(
          [ALL_SITES_DIALOG.RESET_PERMISSIONS, 'SiteGroup', 'Confirm']);

      if (this.actionMenuModel_.item.etldPlus1 !==
          siteGroupToUpdate.etldPlus1) {
        return;
      }
      siteGroupToUpdate.origins.forEach(originEntry => {
        this.resetPermissionsForOrigin_(originEntry.origin);
        if (originEntry.numCookies > 0 || originEntry.usage > 0) {
          originEntry.hasPermissionSettings = false;
          updatedSiteGroup.origins.push(originEntry);
        }
      });
    }

    if (updatedSiteGroup.origins.length > 0) {
      this.set('filteredList_.' + index, updatedSiteGroup);
    } else if (siteGroupToUpdate.numCookies > 0) {
      // If there is no origin for this site group that has any data,
      // but the ETLD+1 has cookies in use, create a origin placeholder
      // for display purposes.
      const originPlaceHolder = {
        origin: `http://${siteGroupToUpdate.etldPlus1}/`,
        engagement: 0,
        usage: 0,
        numCookies: siteGroupToUpdate.numCookies,
        hasPermissionSettings: false
      };
      updatedSiteGroup.origins.push(originPlaceHolder);
      this.set('filteredList_.' + index, updatedSiteGroup);
    } else {
      this.splice('filteredList_', index, 1);
    }

    this.$.allSitesList.fire('iron-resize');
    this.onCloseDialog_(e);
  },

  /**
   * Helper to remove data and cookies for an etldPlus1.
   * @param {!number} index The index of the target siteGroup in filteredList_
   *                        that should be cleared.
   * @private
   */
  clearDataForSiteGroupIndex_: function(index) {
    const siteGroupToUpdate = this.filteredList_[index];
    const updatedSiteGroup = {
      etldPlus1: siteGroupToUpdate.etldPlus1,
      hasInstalledPWA: siteGroupToUpdate.hasInstalledPWA,
      numCookies: 0,
      origins: []
    };

    this.browserProxy.clearEtldPlus1DataAndCookies(siteGroupToUpdate.etldPlus1);

    for (let i = 0; i < siteGroupToUpdate.origins.length; ++i) {
      const updatedOrigin = Object.assign({}, siteGroupToUpdate.origins[i]);
      if (updatedOrigin.hasPermissionSettings) {
        updatedOrigin.numCookies = 0;
        updatedOrigin.usage = 0;
        updatedSiteGroup.origins.push(updatedOrigin);
      }
    }
    this.updateSiteGroup_(index, updatedSiteGroup);
  },

  /**
   * Helper to remove data and cookies for an origin.
   * @param {number} index The index of the target siteGroup in filteredList_
   *                        that should be cleared.
   * @param {string} origin The origin of the target origin
   *                         that should be cleared.
   * @private
   */
  clearDataForOrigin_: function(index, origin) {
    this.browserProxy.clearOriginDataAndCookies(this.toUrl(origin).href);

    const siteGroupToUpdate = this.filteredList_[index];
    const updatedSiteGroup = {
      etldPlus1: siteGroupToUpdate.etldPlus1,
      numCookies: 0,
      origins: []
    };

    const updatedOrigin =
        siteGroupToUpdate.origins.find(o => o.origin === origin);
    if (updatedOrigin.hasPermissionSettings) {
      updatedOrigin.numCookies = 0;
      updatedOrigin.usage = 0;
      updatedSiteGroup.origins = siteGroupToUpdate.origins;
    } else {
      updatedSiteGroup.origins =
          siteGroupToUpdate.origins.filter(o => o.origin !== origin);
    }

    updatedSiteGroup.hasInstalledPWA =
        updatedSiteGroup.origins.some(o => o.isInstalled);
    this.updateSiteGroup_(index, updatedSiteGroup);
  },

  /**
   * Updates the UI after permissions have been reset or data/cookies
   * have been cleared
   * @param {number} index The index of the target siteGroup in filteredList_
   *                        that should be updated.
   * @param {!SiteGroup} updatedSiteGroup The SiteGroup object that represents
   *                                      the new state.
   */
  updateSiteGroup_: function(index, updatedSiteGroup) {
    if (updatedSiteGroup.origins.length > 0) {
      this.set('filteredList_.' + index, updatedSiteGroup);
    } else {
      this.splice('filteredList_', index, 1);
    }
    this.siteGroupMap.delete(updatedSiteGroup.etldPlus1);
  },

  /**
   * Clear data and cookies for an etldPlus1.
   * @param {!Event} e
   * @private
   */
  onClearData_: function(e) {
    const {index, actionScope, origin} = this.actionMenuModel_;
    const scopes = [ALL_SITES_DIALOG.CLEAR_DATA];

    if (actionScope === 'origin') {
      this.browserProxy.recordAction(AllSitesAction2.CLEAR_ORIGIN_DATA);

      const {origins} = this.filteredList_[index];

      scopes.push('Origin');
      const installed =
          (origins.find(o => o.origin === origin) || {}).isInstalled ?
          'Installed' :
          '';
      this.recordUserAction_([...scopes, installed, 'Confirm']);

      this.clearDataForOrigin_(index, origin);
    } else {
      this.browserProxy.recordAction(AllSitesAction2.CLEAR_SITE_GROUP_DATA);

      scopes.push('SiteGroup');
      const {hasInstalledPWA} = this.filteredList_[index];
      const installed = hasInstalledPWA ? 'Installed' : '';
      this.recordUserAction_([...scopes, installed, 'Confirm']);

      this.clearDataForSiteGroupIndex_(index);
    }

    this.$.allSitesList.fire('iron-resize');
    this.updateTotalUsage_();
    this.onCloseDialog_(e);
  },

  /**
   * Clear data and cookies for all sites.
   * @param {!Event} e
   * @private
   */
  onClearAllData_(e) {
    this.browserProxy.recordAction(AllSitesAction2.CLEAR_ALL_DATA);

    const scopes = [ALL_SITES_DIALOG.CLEAR_DATA, 'All'];
    const anyAppsInstalled = this.filteredList_.some(g => g.hasInstalledPWA);
    const installed = anyAppsInstalled ? 'Installed' : '';
    this.recordUserAction_([...scopes, installed, 'Confirm']);

    for (let index = this.filteredList_.length - 1; index >= 0; index--) {
      this.clearDataForSiteGroupIndex_(index);
    }
    this.$.allSitesList.fire('iron-resize');
    this.totalUsage_ = '0 B';
    this.onCloseDialog_(e);
  },
});
