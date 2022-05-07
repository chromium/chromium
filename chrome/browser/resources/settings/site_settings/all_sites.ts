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

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';
import {WebUIListenerMixin, WebUIListenerMixinInterface} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GlobalScrollTargetMixin} from '../global_scroll_target_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';

import {getTemplate} from './all_sites.html.js';
import {AllSitesAction2, AllSitesDialog, ContentSetting, SortMethod} from './constants.js';
import {LocalDataBrowserProxy, LocalDataBrowserProxyImpl} from './local_data_browser_proxy.js';
import {SiteSettingsMixin, SiteSettingsMixinInterface} from './site_settings_mixin.js';
import {OriginInfo, SiteGroup} from './site_settings_prefs_browser_proxy.js';

type ActionMenuModel = {
  actionScope: string,
  index: number,
  item: SiteGroup,
  origin: string,
  isPartitioned: boolean,
  path: string,
  target: HTMLElement,
};

type OpenMenuEvent = CustomEvent<ActionMenuModel>;
type RemoveSiteEvent = CustomEvent<ActionMenuModel>;

type SelectedItem = {
  item: SiteGroup,
  index: number,
};

declare global {
  interface HTMLElementEventMap {
    'open-menu': OpenMenuEvent;
    'remove-site': RemoveSiteEvent;
    'site-entry-selected': CustomEvent<SelectedItem>;
  }
}

export interface AllSitesElement {
  $: {
    allSitesList: IronListElement,
    clearAllButton: HTMLElement,
    confirmClearAllData: CrLazyRenderElement<CrDialogElement>,
    confirmClearData: CrLazyRenderElement<CrDialogElement>,
    confirmRemoveSite: CrLazyRenderElement<CrDialogElement>,
    confirmResetSettings: CrLazyRenderElement<CrDialogElement>,
    listContainer: HTMLElement,
    menu: CrLazyRenderElement<CrActionMenuElement>,
    sortMethod: HTMLSelectElement,
  };
}

// TODO(crbug.com/1234307): Remove when RouteObserverMixin is converted to
// TypeScript.
type Constructor<T> = new (...args: any[]) => T;

const AllSitesElementBaseTemp = GlobalScrollTargetMixin(
    RouteObserverMixin(
        WebUIListenerMixin(I18nMixin(SiteSettingsMixin(PolymerElement)))) as
    unknown as Constructor<PolymerElement>);

const AllSitesElementBase = AllSitesElementBaseTemp as unknown as {
  new (): PolymerElement & I18nMixinInterface & WebUIListenerMixinInterface &
      SiteSettingsMixinInterface & RouteObserverMixinInterface,
};

export class AllSitesElement extends AllSitesElementBase {
  static get is() {
    return 'all-sites';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // TODO(https://crbug.com/1037809): Refactor siteGroupMap to use an Object
      // instead of a Map so that it's observable by Polymer more naturally. As
      // it stands, one cannot use computed properties based off the value of
      // siteGroupMap nor can one use observable functions to listen to changes
      // to siteGroupMap.
      /**
       * Map containing sites to display in the widget, grouped into their
       * eTLD+1 names.
       */
      siteGroupMap: {
        type: Object,
        value() {
          return new Map();
        },
      },

      /**
       * Filtered site group list.
       */
      filteredList_: {
        type: Array,
      },

      /**
       * Needed by GlobalScrollTargetMixin.
       */
      subpageRoute: {
        type: Object,
        value: routes.SITE_SETTINGS_ALL,
        readOnly: true,
      },

      /**
       * The search query entered into the All Sites search textbox. Used to
       * filter the All Sites list.
       */
      filter: {
        type: String,
        value: '',
        observer: 'forceListUpdate_',
      },

      /**
       * All possible sort methods.
       */
      sortMethods_: {
        type: Object,
        value: SortMethod,
        readOnly: true,
      },

      /**
       * Stores the last selected item in the All Sites list.
       */
      selectedItem_: Object,

      /**
       * Used to track the last-focused element across rows for the
       * focusRowBehavior.
       */
      lastFocused_: Object,

      /**
       * Used to track whether the list of row items has been blurred for the
       * focusRowBehavior.
       */
      listBlurred_: Boolean,

      actionMenuModel_: Object,

      /**
       * Used to determine if user is attempting to clear all site data
       * rather than a single site or origin's data.
       */
      clearAllData_: Boolean,

      /**
       * The selected sort method.
       */
      sortMethod_: String,

      /**
       * The total usage of all sites for this profile.
       */
      totalUsage_: {
        type: String,
        value: '0 B',
      },
    };
  }

  siteGroupMap: Map<string, SiteGroup>;
  private filteredList_: Array<SiteGroup>;
  subpageRoute: Route;
  filter: string;
  private selectedItem_: SelectedItem|null;
  private listBlurred_: boolean;
  private actionMenuModel_: ActionMenuModel|null;
  private clearAllData_: boolean;
  private sortMethod_?: SortMethod;
  private totalUsage_: string;
  private localDataBrowserProxy_: LocalDataBrowserProxy =
      LocalDataBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.addWebUIListener(
        'onStorageListFetched', this.onStorageListFetched.bind(this));
    this.addEventListener(
        'site-entry-selected', (e: CustomEvent<SelectedItem>) => {
          this.selectedItem_ = e.detail;
        });

    this.addEventListener('open-menu', this.onOpenMenu_.bind(this));
    this.addEventListener('remove-site', this.onRemoveSite_.bind(this));

    const sortParam = Router.getInstance().getQueryParameters().get('sort');
    if (sortParam !== null &&
        Object.values(SortMethod).includes(sortParam as SortMethod)) {
      this.$.sortMethod.value = sortParam;
    }
    this.sortMethod_ = this.$.sortMethod.value as (SortMethod | undefined);
  }

  override connectedCallback() {
    super.connectedCallback();

    // Set scrollOffset so the iron-list scrolling accounts for the space the
    // title takes.
    afterNextRender(this, () => {
      this.$.allSitesList.scrollOffset = this.$.allSitesList.offsetTop;
    });
  }

  /**
   * Reload the site list when the all sites page is visited.
   *
   * RouteObserverBehavior
   */
  override currentRouteChanged(currentRoute: Route) {
    super.currentRouteChanged(currentRoute);
    if (currentRoute === routes.SITE_SETTINGS_ALL) {
      this.populateList_();
    }
  }

  /**
   * Retrieves a list of all known sites with site details.
   */
  private populateList_() {
    this.browserProxy.getAllSites().then((response) => {
      // Create a new map to make an observable change.
      const newMap = new Map(this.siteGroupMap);
      response.forEach(siteGroup => {
        newMap.set(siteGroup.etldPlus1, siteGroup);
      });
      this.siteGroupMap = newMap;
      this.updateTotalUsage_();
      this.forceListUpdate_();
    });
  }

  /**
   * Integrate sites using storage into the existing sites map, as there
   * may be overlap between the existing sites.
   * @param list The list of sites using storage.
   */
  onStorageListFetched(list: Array<SiteGroup>) {
    // Create a new map to make an observable change.
    const newMap = new Map(this.siteGroupMap);
    list.forEach(storageSiteGroup => {
      newMap.set(storageSiteGroup.etldPlus1, storageSiteGroup);
    });
    this.siteGroupMap = newMap;
    this.updateTotalUsage_();
    this.forceListUpdate_();
    this.focusOnLastSelectedEntry_();
  }

  /**
   * Update the total usage by all sites for this profile after updates
   * to the list
   */
  private updateTotalUsage_() {
    let usageSum = 0;
    for (const [_etldPlus1, siteGroup] of this.siteGroupMap) {
      siteGroup.origins.forEach(origin => {
        usageSum += origin.usage;
      });
    }
    this.browserProxy.getFormattedBytes(usageSum).then(totalUsage => {
      this.totalUsage_ = totalUsage;
    });
  }

  /**
   * Filters the all sites list with the given search query text.
   * @param siteGroupMap The map of sites to filter.
   * @param searchQuery The filter text.
   */
  private filterPopulatedList_(
      siteGroupMap: Map<string, SiteGroup>,
      searchQuery: string): Array<SiteGroup> {
    const result = [];
    for (const [_etldPlus1, siteGroup] of siteGroupMap) {
      if (siteGroup.origins.find(
              originInfo => originInfo.origin.includes(searchQuery))) {
        result.push(siteGroup);
      }
    }
    return this.sortSiteGroupList_(result);
  }

  /**
   * Sorts the given SiteGroup list with the currently selected sort method.
   * @param siteGroupList The list of sites to sort.
   */
  private sortSiteGroupList_(siteGroupList: Array<SiteGroup>):
      Array<SiteGroup> {
    const sortMethod = this.$.sortMethod.value;
    if (!sortMethod) {
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
  }

  /**
   * Comparator used to sort SiteGroups by the amount of engagement the user has
   * with the origins listed inside it. Note only the maximum engagement is used
   * for each SiteGroup (as opposed to the sum) in order to prevent domains with
   * higher numbers of origins from always floating to the top of the list.
   */
  private mostVisitedComparator_(siteGroup1: SiteGroup, siteGroup2: SiteGroup):
      number {
    const getMaxEngagement = (max: number, originInfo: OriginInfo) => {
      return (max > originInfo.engagement) ? max : originInfo.engagement;
    };
    const score1 = siteGroup1.origins.reduce(getMaxEngagement, 0);
    const score2 = siteGroup2.origins.reduce(getMaxEngagement, 0);
    return score2 - score1;
  }

  /**
   * Comparator used to sort SiteGroups by the amount of storage they use. Note
   * this sorts in descending order.
   */
  private storageComparator_(siteGroup1: SiteGroup, siteGroup2: SiteGroup):
      number {
    const getOverallUsage = (siteGroup: SiteGroup) => {
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
  }

  /**
   * Comparator used to sort SiteGroups by their eTLD+1 name (domain).
   */
  private nameComparator_(siteGroup1: SiteGroup, siteGroup2: SiteGroup):
      number {
    return siteGroup1.etldPlus1.localeCompare(siteGroup2.etldPlus1);
  }

  /**
   * Called when the user chooses a different sort method to the default.
   */
  private onSortMethodChanged_() {
    this.sortMethod_ = this.$.sortMethod.value as SortMethod;
    this.filteredList_ = this.sortSiteGroupList_(this.filteredList_);
    // Force the iron-list to rerender its items, as the order has changed.
    this.$.allSitesList.fire('iron-resize');
  }

  /**
   * Forces the all sites list to update its list of items, taking into account
   * the search query and the sort method, then re-renders it.
   */
  private forceListUpdate_() {
    this.filteredList_ =
        this.filterPopulatedList_(this.siteGroupMap, this.filter);
    this.$.allSitesList.fire('iron-resize');
  }

  forceListUpdateForTesting() {
    this.forceListUpdate_();
  }

  /**
   * @return Whether the |siteGroupMap| is empty.
   */
  private siteGroupMapEmpty_(): boolean {
    return !this.siteGroupMap.size;
  }

  /**
   * @return Whether the |filteredList_| is empty due to searching.
   */
  private noSearchResultFound_(): boolean {
    return !this.filteredList_.length && !this.siteGroupMapEmpty_();
  }

  /**
   * Focus on previously selected entry.
   */
  private focusOnLastSelectedEntry_() {
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
  }

  /**
   * Open the overflow menu and ensure that the item is visible in the scroll
   * pane when its menu is opened (it is possible to open off-screen items using
   * keyboard shortcuts).
   */
  private onOpenMenu_(e: OpenMenuEvent) {
    const index = e.detail.index;
    const list = this.$.allSitesList;
    if (index < list.firstVisibleIndex || index > list.lastVisibleIndex) {
      list.scrollToIndex(index);
    }
    const target = e.detail.target;
    this.actionMenuModel_ = e.detail;
    this.$.menu.get().showAt(target);
  }

  onRemoveSite_(e: RemoveSiteEvent) {
    this.actionMenuModel_ = e.detail;
    this.$.confirmRemoveSite.get().showModal();
  }

  onConfirmRemoveSite_(e: Event) {
    const {index, actionScope, origin, isPartitioned} = this.actionMenuModel_!;
    const siteGroupToUpdate = this.filteredList_[index];

    const updatedSiteGroup: SiteGroup = {
      etldPlus1: siteGroupToUpdate.etldPlus1,
      hasInstalledPWA: siteGroupToUpdate.hasInstalledPWA,
      numCookies: siteGroupToUpdate.numCookies,
      origins: []
    };

    if (actionScope === 'origin') {
      if (isPartitioned) {
        this.browserProxy.recordAction(
            AllSitesAction2.REMOVE_ORIGIN_PARTITIONED);
        this.browserProxy.clearPartitionedOriginDataAndCookies(
            this.toUrl(origin)!.href, siteGroupToUpdate.etldPlus1);

      } else {
        this.browserProxy.recordAction(AllSitesAction2.REMOVE_ORIGIN);
        this.browserProxy.clearUnpartitionedOriginDataAndCookies(
            this.toUrl(origin)!.href);
        this.resetPermissionsForOrigin_(origin);
      }
      updatedSiteGroup.origins = siteGroupToUpdate.origins.filter(
          o => (o.isPartitioned !== isPartitioned || o.origin !== origin));

      updatedSiteGroup.hasInstalledPWA =
          updatedSiteGroup.origins.some(o => o.isInstalled);
      updatedSiteGroup.numCookies -=
          siteGroupToUpdate.origins
              .find(
                  o => o.isPartitioned === isPartitioned &&
                      o.origin === origin)!.numCookies;
    } else {
      this.browserProxy.recordAction(AllSitesAction2.REMOVE_SITE_GROUP);
      this.browserProxy.clearEtldPlus1DataAndCookies(
          siteGroupToUpdate.etldPlus1);
      siteGroupToUpdate.origins.forEach(originEntry => {
        this.resetPermissionsForOrigin_(originEntry.origin);
      });
    }

    this.updateSiteGroup_(index, updatedSiteGroup);

    this.$.allSitesList.fire('iron-resize');
    this.updateTotalUsage_();
    this.onCloseDialog_(e);
  }

  /**
   * Confirms the resetting of all content settings for an origin.
   */
  private onConfirmResetSettings_(e: Event) {
    e.preventDefault();
    const scope = this.actionMenuModel_!.actionScope === 'origin' ? 'Origin' :
                                                                    'SiteGroup';
    const scopes = [AllSitesDialog.RESET_PERMISSIONS, scope, 'DialogOpened'];
    this.recordUserAction_(scopes);
    this.$.confirmResetSettings.get().showModal();
  }

  /**
   * Confirms the clearing of all storage data for an etld+1.
   */
  private onConfirmClearData_(e: Event) {
    e.preventDefault();
    const {actionScope, index, origin} = this.actionMenuModel_!;
    const {origins, hasInstalledPWA} = this.filteredList_[index];

    const scope = actionScope === 'origin' ? 'Origin' : 'SiteGroup';
    const appInstalled = actionScope === 'origin' ?
        (origins.find(o => o.origin === origin) || {}).isInstalled :
        hasInstalledPWA;
    const installed = appInstalled ? 'Installed' : '';

    const scopes =
        [AllSitesDialog.CLEAR_DATA, scope, installed, 'DialogOpened'];
    this.recordUserAction_(scopes);
    this.$.confirmClearData.get().showModal();
  }

  /**
   * Confirms the clearing of all storage data for all sites.
   */
  private onConfirmClearAllData_(e: Event) {
    e.preventDefault();
    this.clearAllData_ = true;
    const anyAppsInstalled = this.filteredList_.some(g => g.hasInstalledPWA);
    const scopes = [AllSitesDialog.CLEAR_DATA, 'All'];
    const installed = anyAppsInstalled ? 'Installed' : '';
    this.recordUserAction_([...scopes, installed, 'DialogOpened']);
    this.$.confirmClearAllData.get().showModal();
  }

  private onCloseDialog_(e: Event) {
    chrome.metricsPrivate.recordUserAction('AllSites_DialogClosed');
    (e.target as HTMLElement).closest('cr-dialog')!.close();
    this.actionMenuModel_ = null;
    this.$.menu.get().close();
  }

  /**
   * Get the appropriate label string for the clear data dialog based on whether
   * user is clearing data for an origin or siteGroup, and whether or not the
   * origin/siteGroup has an associated installed app.
   */
  private getClearDataLabel_(): string {
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
  }

  /**
   * Get the appropriate label for the reset permissions confirmation
   * dialog, dependent on whether user is resetting permissions for an
   * origin or an entire SiteGroup.
   */
  private getResetPermissionsLabel_(): string {
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
  }

  private getRemoveSiteTitle_(): string {
    if (this.actionMenuModel_ === null) {
      return '';
    }

    const originScoped = this.actionMenuModel_.actionScope === 'origin';
    const singleOriginSite =
        !originScoped && this.actionMenuModel_.item.origins.length === 1;

    if (this.actionMenuModel_.isPartitioned) {
      assert(originScoped);
      return loadTimeData.substituteString(this.i18n(
          'siteSettingsRemoveSiteOriginPartitionedDialogTitle',
          this.originRepresentation(this.actionMenuModel_.origin),
          this.originRepresentation(this.actionMenuModel_.item.etldPlus1)));
    }

    const numInstalledApps =
        this.actionMenuModel_.item.origins
            .filter(
                o =>
                    !originScoped || this.actionMenuModel_!.origin === o.origin)
            .filter(o => o.isInstalled)
            .length;

    let messageId;
    if (originScoped || singleOriginSite) {
      if (numInstalledApps === 1) {
        messageId = 'siteSettingsRemoveSiteOriginAppDialogTitle';
      } else {
        assert(numInstalledApps === 0);
        messageId = 'siteSettingsRemoveSiteOriginDialogTitle';
      }
    } else {
      if (numInstalledApps > 1) {
        messageId = 'siteSettingsRemoveSiteGroupAppPluralDialogTitle';
      } else if (numInstalledApps === 1) {
        messageId = 'siteSettingsRemoveSiteGroupAppDialogTitle';
      } else {
        messageId = 'siteSettingsRemoveSiteGroupDialogTitle';
      }
    }

    let displayOrigin;
    if (originScoped) {
      displayOrigin = this.actionMenuModel_.origin;
    } else if (singleOriginSite) {
      displayOrigin = this.actionMenuModel_.item.origins[0].origin;
    } else {
      displayOrigin = this.actionMenuModel_.item.etldPlus1;
    }

    return loadTimeData.substituteString(
        this.i18n(messageId), this.originRepresentation(displayOrigin));
  }

  private getRemoveSiteLogoutBulletPoint_() {
    if (this.actionMenuModel_ === null) {
      return '';
    }

    const originScoped = this.actionMenuModel_.actionScope === 'origin';
    const singleOriginSite =
        !originScoped && this.actionMenuModel_.item.origins.length === 1;

    return originScoped || singleOriginSite ?
        this.i18n('siteSettingsRemoveSiteOriginLogout') :
        this.i18n('siteSettingsRemoveSiteGroupLogout');
  }

  private showPermissionsBulletPoint_(): boolean {
    if (this.actionMenuModel_ === null) {
      return false;
    }

    // If the selected item if a site group, search all child origins for
    // permissions. If it is not, only look at the relevant origin.
    return this.actionMenuModel_.item.origins
        .filter(
            o => this.actionMenuModel_!.actionScope !== 'origin' ||
                this.actionMenuModel_!.origin === o.origin)
        .some(o => o.hasPermissionSettings);
  }

  /**
   * Get the appropriate label for the clear all data confirmation
   * dialog, depending on whether or not any apps are installed.
   */
  private getClearAllDataLabel_(): string {
    const anyAppsInstalled = this.filteredList_.some(g => g.hasInstalledPWA);
    const messageId = anyAppsInstalled ?
        'siteSettingsClearAllStorageConfirmationInstalled' :
        'siteSettingsClearAllStorageConfirmation';
    return loadTimeData.substituteString(
        this.i18n(messageId), this.totalUsage_);
  }

  /**
   * Get the appropriate label for the clear data confirmation
   * dialog, depending on whether the user is clearing data for a
   * single origin or an entire site group.
   */
  private getLogoutLabel_(): string {
    return this.actionMenuModel_!.actionScope === 'origin' ?
        this.i18n('siteSettingsSiteClearStorageSignOut') :
        this.i18n('siteSettingsSiteGroupDeleteSignOut');
  }

  private recordUserAction_(scopes: Array<string>) {
    chrome.metricsPrivate.recordUserAction(
        ['AllSites', ...scopes].filter(Boolean).join('_'));
  }

  /**
   * Resets all permission settings for a single origin.
   */
  private resetPermissionsForOrigin_(origin: string) {
    this.browserProxy.setOriginPermissions(
        origin, null, ContentSetting.DEFAULT);
  }

  /**
   * Resets all permissions for a single origin or all origins listed in
   * |siteGroup.origins|.
   */
  private onResetSettings_(e: Event) {
    const {actionScope, index, origin} = this.actionMenuModel_!;
    const siteGroupToUpdate = this.filteredList_[index];

    const updatedSiteGroup: SiteGroup = {
      etldPlus1: siteGroupToUpdate.etldPlus1,
      hasInstalledPWA: false,
      numCookies: siteGroupToUpdate.numCookies,
      origins: [],
    };

    if (actionScope === 'origin') {
      this.browserProxy.recordAction(AllSitesAction2.RESET_ORIGIN_PERMISSIONS);
      this.recordUserAction_(
          [AllSitesDialog.RESET_PERMISSIONS, 'Origin', 'Confirm']);

      this.resetPermissionsForOrigin_(origin);
      updatedSiteGroup.origins = siteGroupToUpdate.origins;
      const updatedOrigin =
          updatedSiteGroup.origins.find(o => o.origin === origin)!;
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
          [AllSitesDialog.RESET_PERMISSIONS, 'SiteGroup', 'Confirm']);

      if (this.actionMenuModel_!.item.etldPlus1 !==
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
        hasPermissionSettings: false,
        isInstalled: false,
        isPartitioned: false,
      };
      updatedSiteGroup.origins.push(originPlaceHolder);
      this.set('filteredList_.' + index, updatedSiteGroup);
    } else {
      this.splice('filteredList_', index, 1);
    }

    this.$.allSitesList.fire('iron-resize');
    this.onCloseDialog_(e);
  }

  /**
   * Helper to remove data and cookies for an etldPlus1.
   * @param index The index of the target siteGroup in filteredList_ that should
   *     be cleared.
   */
  private clearDataForSiteGroupIndex_(index: number) {
    const siteGroupToUpdate = this.filteredList_[index];
    const updatedSiteGroup: SiteGroup = {
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
  }

  /**
   * Helper to remove data and cookies for an origin.
   * @param index The index of the target siteGroup in filteredList_ that should
   *     be cleared.
   * @param origin The origin of the target origin that should be cleared.
   */
  private clearDataForOrigin_(index: number, origin: string) {
    this.browserProxy.clearUnpartitionedOriginDataAndCookies(
        this.toUrl(origin)!.href);

    const siteGroupToUpdate = this.filteredList_[index];
    const updatedSiteGroup: SiteGroup = {
      etldPlus1: siteGroupToUpdate.etldPlus1,
      hasInstalledPWA: false,
      numCookies: 0,
      origins: [],
    };

    const updatedOrigin =
        siteGroupToUpdate.origins.find(o => o.origin === origin)!;
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
  }

  /**
   * Updates the UI after permissions have been reset or data/cookies
   * have been cleared
   * @param index The index of the target siteGroup in filteredList_ that should
   *     be updated.
   * @param updatedSiteGroup The SiteGroup object that represents the new state.
   */
  private updateSiteGroup_(index: number, updatedSiteGroup: SiteGroup) {
    if (updatedSiteGroup.origins.length > 0) {
      this.set('filteredList_.' + index, updatedSiteGroup);
    } else {
      this.splice('filteredList_', index, 1);
    }
    this.siteGroupMap.delete(updatedSiteGroup.etldPlus1);
  }

  /**
   * Clear data and cookies for an etldPlus1.
   */
  private onClearData_(e: Event) {
    const {index, actionScope, origin} = this.actionMenuModel_!;
    const scopes: Array<string> = [AllSitesDialog.CLEAR_DATA];

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
  }

  /**
   * Clear data and cookies for all sites.
   */
  private onClearAllData_(e: Event) {
    this.browserProxy.recordAction(AllSitesAction2.CLEAR_ALL_DATA);

    const scopes = [AllSitesDialog.CLEAR_DATA, 'All'];
    const anyAppsInstalled = this.filteredList_.some(g => g.hasInstalledPWA);
    const installed = anyAppsInstalled ? 'Installed' : '';
    this.recordUserAction_([...scopes, installed, 'Confirm']);

    for (let index = this.filteredList_.length - 1; index >= 0; index--) {
      this.clearDataForSiteGroupIndex_(index);
    }
    this.$.allSitesList.fire('iron-resize');
    this.totalUsage_ = '0 B';
    this.onCloseDialog_(e);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'all-sites': AllSitesElement;
  }
}

customElements.define(AllSitesElement.is, AllSitesElement);
