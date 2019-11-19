// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'all-sites' is the polymer element for showing the list of all sites under
 * Site Settings.
 */
Polymer({
  is: 'all-sites',

  behaviors: [
    SiteSettingsBehavior,
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
    settings.GlobalScrollTargetBehavior,
  ],

  properties: {
    /**
     * Map containing sites to display in the widget, grouped into their
     * eTLD+1 names.
     * @type {!Map<string, !SiteGroup>}
     */
    siteGroupMap: {
      type: Object,
      value: function() {
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
      value: settings.routes.SITE_SETTINGS_ALL,
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
      value: settings.SortMethod,
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
     *   index: number,
     *   item: !SiteGroup,
     *   path: string,
     *   target: !HTMLElement
     * }}
     */
    actionMenuModel_: Object,

    /**
     * The selected sort method.
     * @type {!settings.SortMethod|undefined}
     * @private
     */
    sortMethod_: String,
  },

  /** @private {?settings.LocalDataBrowserProxy} */
  localDataBrowserProxy_: null,

  /** @override */
  created: function() {
    this.localDataBrowserProxy_ =
        settings.LocalDataBrowserProxyImpl.getInstance();
  },

  listeners: {
    'open-menu': 'onOpenMenu_',
  },

  /** @override */
  ready: function() {
    this.addWebUIListener(
        'onStorageListFetched', this.onStorageListFetched.bind(this));
    this.addEventListener('site-entry-selected', e => {
      const event =
          /** @type {!CustomEvent<!{item: !SiteGroup, index: number}>} */ (e);
      this.selectedItem_ = event.detail;
    });
    this.sortMethod_ = this.$.sortMethod.value;
  },

  /** @override */
  attached: function() {
    // Set scrollOffset so the iron-list scrolling accounts for the space the
    // title takes.
    Polymer.RenderStatus.afterNextRender(this, () => {
      this.$.allSitesList.scrollOffset = this.$.allSitesList.offsetTop;
    });
  },

  /**
   * Reload the site list when the all sites page is visited.
   *
   * settings.RouteObserverBehavior
   * @param {!settings.Route} currentRoute
   * @protected
   */
  currentRouteChanged: function(currentRoute) {
    settings.GlobalScrollTargetBehaviorImpl.currentRouteChanged.call(
        this, currentRoute);
    if (currentRoute == settings.routes.SITE_SETTINGS_ALL) {
      this.populateList_();
    }
  },

  /**
   * Retrieves a list of all known sites with site details.
   * @private
   */
  populateList_: function() {
    /** @type {!Array<settings.ContentSettingsTypes>} */
    const contentTypes = this.getCategoryList();
    // Make sure to include cookies, because All Sites handles data storage +
    // cookies as well as regular settings.ContentSettingsTypes.
    if (!contentTypes.includes(settings.ContentSettingsTypes.COOKIES)) {
      contentTypes.push(settings.ContentSettingsTypes.COOKIES);
    }

    this.browserProxy.getAllSites(contentTypes).then((response) => {
      // Create a new map to make an observable change.
      const newMap = /** @type {!Map<string, !SiteGroup>} */
                      (new Map(this.siteGroupMap));
      response.forEach(siteGroup => {
        newMap.set(siteGroup.etldPlus1, siteGroup);
      });
      this.siteGroupMap = newMap;
      this.forceListUpdate_();
    });
  },

  /**
   * Integrate sites using storage into the existing sites map, as there
   * may be overlap between the existing sites.
   * @param {!Array<!SiteGroup>} list The list of sites using storage.
   */
  onStorageListFetched: function(list) {
    // Create a new map to make an observable change.
    const newMap = /** @type {!Map<string, !SiteGroup>} */
                    (new Map(this.siteGroupMap));
    list.forEach(storageSiteGroup => {
      newMap.set(storageSiteGroup.etldPlus1, storageSiteGroup);
    });
    this.siteGroupMap = newMap;
    this.forceListUpdate_();
    this.focusOnLastSelectedEntry_();
  },

  /**
   * Filters the all sites list with the given search query text.
   * @param {!Map<string, !SiteGroup>} siteGroupMap The map of sites to filter.
   * @param {string} searchQuery The filter text.
   * @return {!Array<!SiteGroup>}
   * @private
   */
  filterPopulatedList_: function(siteGroupMap, searchQuery) {
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
  sortSiteGroupList_: function(siteGroupList) {
    const sortMethod = this.$.sortMethod.value;
    if (!this.sortMethods_) {
      return siteGroupList;
    }

    if (sortMethod == settings.SortMethod.MOST_VISITED) {
      siteGroupList.sort(this.mostVisitedComparator_);
    } else if (sortMethod == settings.SortMethod.STORAGE) {
      siteGroupList.sort(this.storageComparator_);
    } else if (sortMethod == settings.SortMethod.NAME) {
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
  mostVisitedComparator_: function(siteGroup1, siteGroup2) {
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
  storageComparator_: function(siteGroup1, siteGroup2) {
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
  nameComparator_: function(siteGroup1, siteGroup2) {
    return siteGroup1.etldPlus1.localeCompare(siteGroup2.etldPlus1);
  },

  /**
   * Called when the user chooses a different sort method to the default.
   * @private
   */
  onSortMethodChanged_: function() {
    this.sortMethod_ = this.$.sortMethod.value;
    this.filteredList_ =
        this.sortSiteGroupList_(this.filteredList_);
    // Force the iron-list to rerender its items, as the order has changed.
    this.$.allSitesList.fire('iron-resize');
  },

  /**
   * Forces the all sites list to update its list of items, taking into account
   * the search query and the sort method, then re-renders it.
   * @private
   */
  forceListUpdate_: function() {
    this.filteredList_ =
        this.filterPopulatedList_(this.siteGroupMap, this.filter);
    this.$.allSitesList.fire('iron-resize');
  },

  /**
   * Whether the |siteGroupMap| is empty.
   * @return {boolean}
   * @private
   */
  siteGroupMapEmpty_: function() {
    return !this.siteGroupMap.size;
  },

  /**
   * Whether the |filteredList_| is empty due to searching.
   * @return {boolean}
   * @private
   */
  noSearchResultFound_: function() {
    return !this.filteredList_.length && !this.siteGroupMapEmpty_();
  },

  /**
   * Focus on previously selected entry.
   * @private
   */
  focusOnLastSelectedEntry_: function() {
    if (this.selectedItem_ == null || this.siteGroupMap.size == 0) {
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
   *    index: number, item: !SiteGroup,
   *    path: string, target: !HTMLElement
   *    }>} e
   * @private
   */
  onOpenMenu_: function(e) {
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
  onConfirmResetSettings_: function(e) {
    e.preventDefault();
    this.$.confirmResetSettings.get().showModal();
  },

  /**
   * Confirms the clearing of all storage data for an etld+1.
   * @param {!Event} e
   * @private
   */
  onConfirmClearData_: function(e) {
    e.preventDefault();
    this.$.confirmClearData.get().showModal();
  },

  /** @private */
  onCloseDialog_: function(e) {
    e.target.closest('cr-dialog').close();
    this.actionMenuModel_ = null;
    this.$.menu.get().close();
  },

  /**
   * Formats the |label| string with |name|, using $<num> as markers.
   * @param {string} label
   * @param {string} name
   * @return {string}
   * @private
   */
  getFormatString_: function(label, name) {
    return loadTimeData.substituteString(label, name);
  },

  /**
   * Resets all permissions for all origins listed in |siteGroup.origins|.
   * @param {!Event} e
   * @private
   */
  onResetSettings_: function(e) {
    const contentSettingsTypes = this.getCategoryList();
    const index = this.actionMenuModel_.index;
    this.browserProxy.recordAction(settings.AllSitesAction.RESET_PERMISSIONS);
    if (this.actionMenuModel_.item.etldPlus1 !=
        this.filteredList_[index].etldPlus1) {
      return;
    }
    for (let i = 0; i < this.filteredList_[index].origins.length; ++i) {
      const origin = this.filteredList_[index].origins[i].origin;
      this.browserProxy.setOriginPermissions(
          origin, contentSettingsTypes, settings.ContentSetting.DEFAULT);
      if (contentSettingsTypes.includes(
              settings.ContentSettingsTypes.PLUGINS)) {
        this.browserProxy.clearFlashPref(origin);
      }
      this.filteredList_[index].origins[i].hasPermissionSettings = false;
    }
    const updatedSiteGroup = {
      etldPlus1: this.filteredList_[index].etldPlus1,
      numCookies: this.filteredList_[index].numCookies,
      origins: []
    };
    for (let i = 0; i < this.filteredList_[index].origins.length; ++i) {
      const updatedOrigin =
          Object.assign({}, this.filteredList_[index].origins[i]);
      if (updatedOrigin.numCookies > 0 || updatedOrigin.usage > 0) {
        updatedOrigin.hasPermissionSettings = false;
        updatedSiteGroup.origins.push(updatedOrigin);
      }
    }
    if (updatedSiteGroup.origins.length > 0) {
      this.set('filteredList_.' + index, updatedSiteGroup);
    } else if (this.filteredList_[index].numCookies > 0) {
      // If there is no origin for this site group that has any data,
      // but the ETLD+1 has cookies in use, create a origin placeholder
      // for display purposes.
      const originPlaceHolder = {
        origin: 'http://' + this.filteredList_[index].etldPlus1 + '/',
        engagement: 0,
        usage: 0,
        numCookies: this.filteredList_[index].numCookies,
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
   * Clear data and cookies for an etldPlus1.
   * @param {!Event} e
   * @private
   */
  onClearData_: function(e) {
    const index = this.actionMenuModel_.index;
    // Clean up the SiteGroup.
    this.browserProxy.clearEtldPlus1DataAndCookies(
        this.filteredList_[index].etldPlus1);
    const updatedSiteGroup = {
      etldPlus1: this.filteredList_[index].etldPlus1,
      numCookies: 0,
      origins: []
    };
    for (let i = 0; i < this.filteredList_[index].origins.length; ++i) {
      const updatedOrigin =
          Object.assign({}, this.filteredList_[index].origins[i]);
      if (updatedOrigin.hasPermissionSettings) {
        updatedOrigin.numCookies = 0;
        updatedOrigin.usage = 0;
        updatedSiteGroup.origins.push(updatedOrigin);
      }
    }
    if (updatedSiteGroup.origins.length > 0) {
      this.set('filteredList_.' + index, updatedSiteGroup);
    } else {
      this.splice('filteredList_', index, 1);
    }
    this.$.allSitesList.fire('iron-resize');
    this.onCloseDialog_(e);
  },
});
