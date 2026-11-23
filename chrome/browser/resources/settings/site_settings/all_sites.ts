// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'all-sites' is the Lit element for showing the list of all sites under
 * Site Settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/cr_infinite_list/cr_infinite_list.js';
import '../settings_page/settings_subpage.js';
import './all_sites_icons.html.js';
import './site_entry.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {GlobalScrollTargetMixinLit} from '../global_scroll_target_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {DeleteBrowsingDataAction, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {Router} from '../router.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import {getCss} from './all_sites.css.js';
import {getHtml} from './all_sites.html.js';
import {AllSitesAction2, AllSitesDialog, ContentSetting, SortMethod} from './constants.js';
import type {OriginInfo, SiteGroup} from './site_settings_browser_proxy.js';
import {SiteSettingsMixinLit} from './site_settings_mixin_lit.js';

interface ActionMenuModel {
  actionScope: string;
  index: number;
  item: SiteGroup;
  origin: string;
  isPartitioned: boolean;
  path: string;
  target: HTMLElement;
}

type RemoveSiteEvent = CustomEvent<ActionMenuModel>;

interface SelectedItem {
  item: SiteGroup;
  index: number;
}

declare global {
  interface HTMLElementEventMap {
    'remove-site': RemoveSiteEvent;
    'site-entry-selected': CustomEvent<SelectedItem>;
  }
}

export interface AllSitesElement {
  $: {
    clearAllButton: HTMLElement,
    clearLabel: HTMLElement,
    confirmClearAllData: CrLazyRenderLitElement<CrDialogElement>,
    confirmRemoveSite: CrLazyRenderLitElement<CrDialogElement>,
    listContainer: HTMLElement,
    sortMethod: HTMLSelectElement,
  };
}

const AllSitesElementBase = GlobalScrollTargetMixinLit(SettingsViewMixinLit(
    WebUiListenerMixinLit(I18nMixinLit(SiteSettingsMixinLit(CrLitElement)))));

export class AllSitesElement extends AllSitesElementBase {
  static get is() {
    return 'all-sites';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Map containing sites to display in the widget, grouped into their
       * group names.
       */
      siteGroupMap: {type: Object},

      /**
       * Filtered site group list.
       */
      filteredList_: {type: Array},

      /**
       * The search query entered into the All Sites search textbox. Used to
       * filter the All Sites list.
       */
      filter: {type: String},

      /**
       * Used to track the last-focused element across rows for the
       * FocusRowMixinLit.
       */
      lastFocused_: {type: Object},

      /**
       * Used to track whether the list of row items has been blurred for the
       * FocusRowMixinLit.
       */
      listBlurred_: {type: Boolean},

      actionMenuModel_: {type: Object},

      /**
       * The selected sort method.
       */
      sortMethod_: {type: String},

      /**
       * The total usage of all sites for this profile.
       */
      totalUsage_: {type: String},
    };
  }

  accessor siteGroupMap: Map<string, SiteGroup> = new Map();
  protected accessor filteredList_: SiteGroup[] = [];
  accessor filter: string = '';
  protected accessor lastFocused_: HTMLElement|null = null;
  protected accessor listBlurred_: boolean = false;
  private accessor actionMenuModel_: ActionMenuModel|null = null;
  protected accessor sortMethod_: SortMethod = SortMethod.MOST_VISITED;
  private accessor totalUsage_: string = '0 B';

  override subpageRoute: Route = routes.SITE_SETTINGS_ALL;
  private metricsBrowserProxy: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  /** The last selected item in the All Sites list. */
  private selectedItem_: SelectedItem|null = null;

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'onStorageListFetched', this.onStorageListFetched.bind(this));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('filter')) {
      this.forceListUpdate_();
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);

    this.addEventListener(
        'site-entry-selected', (e: CustomEvent<SelectedItem>) => {
          this.selectedItem_ = e.detail;
        });
    this.addEventListener('remove-site', this.onRemoveSite_.bind(this));

    const sortParam = Router.getInstance().getQueryParameters().get('sort');
    if (sortParam !== null &&
        Object.values(SortMethod).includes(sortParam as SortMethod)) {
      this.sortMethod_ = sortParam as SortMethod;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('subpageScrollTarget') &&
        this.subpageScrollTarget) {
      // Set scrollOffset so the cr-infinite-list scrolling accounts for the
      // space the title takes.
      const allSitesList = this.shadowRoot.querySelector('cr-infinite-list');
      assert(allSitesList);
      allSitesList.scrollOffset = allSitesList.offsetTop;
    }
  }

  /**
   * Reload the site list when the all sites page is visited.
   *
   * RouteObserverMixin
   */
  override currentRouteChanged(currentRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(currentRoute, oldRoute);
    if (currentRoute === routes.SITE_SETTINGS_ALL &&
        currentRoute !== oldRoute) {
      this.populateList_();
    }
  }

  /**
   * Retrieves a list of all known sites with site details.
   */
  private populateList_() {
    this.browserProxy.getAllSites().then((response) => {
      // Start from an empty map so sites whose permissions and storage were
      // cleared on another page do not remain in the list when reloading.
      const newMap = new Map<string, SiteGroup>();
      response.forEach(siteGroup => {
        newMap.set(siteGroup.groupingKey, siteGroup);
      });
      this.siteGroupMap = newMap;
      this.forceListUpdate_();
    });
  }

  /**
   * Integrate sites using storage into the existing sites map, as there
   * may be overlap between the existing sites.
   * @param list The list of sites using storage.
   */
  onStorageListFetched(list: SiteGroup[]) {
    // Create a new map to make an observable change.
    const newMap = new Map(this.siteGroupMap);
    list.forEach(storageSiteGroup => {
      newMap.set(storageSiteGroup.groupingKey, storageSiteGroup);
    });
    this.siteGroupMap = newMap;
    this.forceListUpdate_();
    this.focusOnLastSelectedEntry_();
  }

  /**
   * Update the total usage by all sites for this profile after updates
   * to the list
   */
  private updateTotalUsage_() {
    let usageSum = 0;
    for (const siteGroup of this.filteredList_) {
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
      siteGroupMap: Map<string, SiteGroup>, searchQuery: string): SiteGroup[] {
    const result: SiteGroup[] = [];
    for (const [_groupingKey, siteGroup] of siteGroupMap) {
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
  private sortSiteGroupList_(siteGroupList: SiteGroup[]): SiteGroup[] {
    const sortMethod = this.sortMethod_;
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
    return siteGroup1.displayName.localeCompare(siteGroup2.displayName);
  }

  protected isSortMethodSelected_(sortMethod: SortMethod): boolean {
    return sortMethod === this.sortMethod_;
  }

  /**
   * Called when the user chooses a different sort method to the default.
   */
  protected onSortMethodChange_() {
    this.sortMethod_ = this.$.sortMethod.value as SortMethod;
    this.filteredList_ = this.sortSiteGroupList_([...this.filteredList_]);
  }

  /**
   * Forces the all sites list to update its list of items, taking into account
   * the search query and the sort method, then re-renders it.
   */
  private forceListUpdate_() {
    this.filteredList_ =
        this.filterPopulatedList_(this.siteGroupMap, this.filter);
    this.updateTotalUsage_();
  }

  forceListUpdateForTesting() {
    this.forceListUpdate_();
  }

  /**
   * @return Whether the |siteGroupMap| is empty.
   */
  protected siteGroupMapEmpty_(): boolean {
    return !this.siteGroupMap.size;
  }

  /**
   * @return Whether the |filteredList_| is empty due to searching.
   */
  protected noSearchResultFound_(): boolean {
    return !this.filteredList_.length && !this.siteGroupMapEmpty_();
  }

  /**
   * Focus on previously selected entry.
   */
  private async focusOnLastSelectedEntry_() {
    if (!this.selectedItem_ || this.filteredList_.length === 0) {
      return;
    }
    // The index is used instead of the item, in case the item was already
    // removed.
    const index = Math.max(
        0, Math.min(this.selectedItem_.index, this.filteredList_.length - 1));
    this.selectedItem_ = null;
    // Wait for the list to receive the updated items before asking it to
    // render the entry.
    await this.updateComplete;
    const allSitesList = this.shadowRoot.querySelector('cr-infinite-list');
    if (!allSitesList) {
      return;
    }
    await allSitesList.updateComplete;
    const element = await allSitesList.ensureItemRendered(index);
    element.focus();
  }

  protected shouldShowClearAllButton_(): boolean {
    return this.filteredList_.length > 0;
  }

  private onRemoveSite_(e: RemoveSiteEvent) {
    this.actionMenuModel_ = e.detail;
    this.$.confirmRemoveSite.get().showModal();
  }

  // Creates a placeholder origin used to hold cookies scoped at the eTLD+1
  // level.
  private generatePlaceholderOrigin_(
      numCookies: number, origin: string, etldPlus1?: string): OriginInfo {
    return {
      origin: etldPlus1 ? `http://${etldPlus1}/` : origin,
      engagement: 0,
      usage: 0,
      numCookies: numCookies,
      hasPermissionSettings: false,
      isInstalled: false,
      isPartitioned: false,
    };
  }

  protected onConfirmRemoveSiteClick_(e: Event) {
    const {index, actionScope, origin, isPartitioned} = this.actionMenuModel_!;
    const siteGroupToUpdate = this.filteredList_[index];

    const updatedSiteGroup: SiteGroup = {
      ...siteGroupToUpdate,
      origins: [],
    };

    this.metricsBrowserProxy.recordDeleteBrowsingDataAction(
        DeleteBrowsingDataAction.SITES_SETTINGS_PAGE);

    if (actionScope === 'origin') {
      if (isPartitioned) {
        this.browserProxy.recordAction(
            AllSitesAction2.REMOVE_ORIGIN_PARTITIONED);
        this.browserProxy.clearPartitionedOriginDataAndCookies(
            this.toUrl(origin)!.href, siteGroupToUpdate.groupingKey);

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
      if (updatedSiteGroup.origins.length === 0 &&
          updatedSiteGroup.numCookies > 0) {
        const originPlaceHolder = this.generatePlaceholderOrigin_(
            updatedSiteGroup.numCookies, origin, updatedSiteGroup.etldPlus1);
        updatedSiteGroup.origins.push(originPlaceHolder);
      }
    } else {
      this.browserProxy.recordAction(AllSitesAction2.REMOVE_SITE_GROUP);
      this.browserProxy.clearSiteGroupDataAndCookies(
          siteGroupToUpdate.groupingKey);
      siteGroupToUpdate.origins.forEach(originEntry => {
        this.resetPermissionsForOrigin_(originEntry.origin);
      });
    }

    this.updateSiteGroup_(index, updatedSiteGroup);

    this.updateTotalUsage_();
    this.closeDialog_(e);
  }

  /**
   * Checks if a filter is applied.
   * @return True if a filter is applied.
   */
  private isFiltered_(): boolean {
    return this.filter !== '';
  }

  /**
   * Selects the appropriate string to display for clear button based on whether
   * a filter is applied.
   * @return The appropriate |clearAllButton| string based on whether a filter
   *     is applied.
   */
  protected getClearDataButtonString_(): string {
    return this.i18n(
        this.isFiltered_() ? 'siteSettingsDeleteDisplayedStorageLabel' :
                             'siteSettingsDeleteAllStorageLabel');
  }

  /**
   * Selects the appropriate string to display for total usage based on whether
   * a filter is applied.
   * @return The appropriate |clearLabel| string based on whether a filter
   *     is applied.
   */
  protected getClearStorageDescription_(): string {
    const descriptionId = this.isFiltered_() ?
        'siteSettingsClearDisplayedStorageDescription' :
        'siteSettingsClearAllStorageDescription';
    return loadTimeData.substituteString(
        this.i18n(descriptionId), this.totalUsage_);
  }

  /**
   * Confirms the clearing of all storage data for all sites.
   */
  protected onConfirmClearAllDataClick_(e: Event) {
    e.preventDefault();
    const anyAppsInstalled = this.filteredList_.some(g => g.hasInstalledPWA);
    const scopes = [AllSitesDialog.CLEAR_DATA, 'All'];
    const installed = anyAppsInstalled ? 'Installed' : '';
    this.recordUserAction_([...scopes, installed, 'DialogOpened']);
    this.$.confirmClearAllData.get().showModal();
  }

  private closeDialog_(e: Event) {
    chrome.metricsPrivate.recordUserAction('AllSites_DialogClosed');
    (e.target as HTMLElement).closest('cr-dialog')!.close();
    this.actionMenuModel_ = null;
  }

  protected onCloseDialogClick_(e: Event) {
    this.closeDialog_(e);
  }

  protected getRemoveSiteTitle_(): string {
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
          this.actionMenuModel_.item.displayName));
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
      displayOrigin = this.actionMenuModel_.item.displayName;
    }

    return loadTimeData.substituteString(
        this.i18n(messageId), this.originRepresentation(displayOrigin));
  }

  protected getRemoveSiteLogoutBulletPoint_(): string {
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

  protected shouldShowPermissionsBulletPoint_(): boolean {
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
   * Selects the appropriate title to display for clear storage confirmation
   * dialog based on whether a filter is applied.
   * @return The appropriate title for clear storage confirmation dialog.
   */
  protected getClearAllStorageDialogTitle_(): string {
    return this.i18n(
        this.isFiltered_() ? 'siteSettingsDeleteDisplayedStorageDialogTitle' :
                             'siteSettingsDeleteAllStorageDialogTitle');
  }

  /**
   * Get the appropriate label for the clear data confirmation dialog, depending
   * on whether any apps are installed and/or a filter is applied.
   * @return The appropriate description for clear data confirmation dialog.
   */
  protected getClearAllStorageDialogDescription_(): string {
    const anyAppsInstalled = this.filteredList_.some(g => g.hasInstalledPWA);

    let messageId;
    if (anyAppsInstalled) {
      messageId = this.isFiltered_() ?
          'siteSettingsDeleteDisplayedStorageConfirmationInstalled' :
          'siteSettingsDeleteAllStorageConfirmationInstalled';
    } else {
      messageId = this.isFiltered_() ?
          'siteSettingsDeleteDisplayedStorageConfirmation' :
          'siteSettingsDeleteAllStorageConfirmation';
    }

    return loadTimeData.substituteString(
        this.i18n(messageId), this.totalUsage_);
  }

  /**
   * Selects the appropriate string to display for the sign-out string in
   * confirmation popup based on whether a filter is applied.
   * @return The appropriate sign out confirmation string based on whether a
   *     filter is applied.
   */
  protected getClearAllStorageDialogSignOutLabel_(): string {
    return this.i18n(
        this.isFiltered_() ? 'siteSettingsClearDisplayedStorageSignOut' :
                             'siteSettingsClearAllStorageSignOut');
  }

  private recordUserAction_(scopes: string[]) {
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
   * Helper to remove data and cookies for a group.
   * @param index The index of the target siteGroup in filteredList_ that should
   *     be cleared.
   */
  private clearDataForSiteGroupIndex_(index: number) {
    const siteGroupToUpdate = this.filteredList_[index];
    const updatedSiteGroup: SiteGroup = {
      ...siteGroupToUpdate,
      numCookies: 0,
      origins: [],
    };

    this.browserProxy.clearSiteGroupDataAndCookies(
        siteGroupToUpdate.groupingKey);

    for (let i = 0; i < siteGroupToUpdate.origins.length; ++i) {
      const updatedOrigin = {...siteGroupToUpdate.origins[i]};
      if (updatedOrigin.hasPermissionSettings) {
        updatedOrigin.numCookies = 0;
        updatedOrigin.usage = 0;
        updatedSiteGroup.origins.push(updatedOrigin);
      }
    }
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
      this.filteredList_[index] = updatedSiteGroup;
      this.siteGroupMap.set(updatedSiteGroup.groupingKey, updatedSiteGroup);
      this.filteredList_ = [...this.filteredList_];
    } else {
      this.filteredList_.splice(index, 1);
      this.siteGroupMap.delete(updatedSiteGroup.groupingKey);
      this.filteredList_ = [...this.filteredList_];
    }
  }

  /**
   * Clear data and cookies for all sites.
   */
  protected onClearAllDataClick_(e: Event) {
    this.browserProxy.recordAction(AllSitesAction2.CLEAR_ALL_DATA);
    const scopes = [AllSitesDialog.CLEAR_DATA, 'All'];
    const anyAppsInstalled = this.filteredList_.some(g => g.hasInstalledPWA);
    const installed = anyAppsInstalled ? 'Installed' : '';
    this.recordUserAction_([...scopes, installed, 'Confirm']);
    this.metricsBrowserProxy.recordDeleteBrowsingDataAction(
        DeleteBrowsingDataAction.SITES_SETTINGS_PAGE);
    for (let index = this.filteredList_.length - 1; index >= 0; index--) {
      this.clearDataForSiteGroupIndex_(index);
    }
    // Needed to update the filteredList_ for the "No sites found" text to
    // appear.
    this.forceListUpdate_();
    this.totalUsage_ = '0 B';
    this.closeDialog_(e);
  }

  protected onRestoreListFocus_() {
    this.listBlurred_ = false;
  }

  protected onLastFocusedChanged_(e: CustomEvent<{value: HTMLElement | null}>) {
    this.lastFocused_ = e.detail.value;
  }

  protected onListBlurredChanged_(e: CustomEvent<{value: boolean}>) {
    this.listBlurred_ = e.detail.value;
  }

  protected onSearchTermChanged_(e: CustomEvent<{value: string}>) {
    this.filter = e.detail.value;
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'all-sites': AllSitesElement;
  }
}

customElements.define(AllSitesElement.is, AllSitesElement);
