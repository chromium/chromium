// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/history_clusters/clusters.js';
import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
// <if expr="not is_chromeos">
import './history_cross_device_signin_promo.js';
import './history_sync_promo.js';
// </if>
import './history_list.js';
import './history_toolbar.js';
import './history_filter_chips.js';
import './query_manager.js';
import './router.js';
import './side_bar.js';
import './synced_device_manager.js';
import '/strings.m.js';

import {ColorChangeUpdater, COLORS_CSS_SELECTOR} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import {HistoryResultType} from 'chrome://resources/cr_components/history/constants.js';
import type {ForeignSession, ForeignSessionPageCallbackRouter} from 'chrome://resources/cr_components/history/foreign_sessions.mojom-webui.js';
import {browserProxyFactory as foreignSessionBrowserProxyFactory} from 'chrome://resources/cr_components/history/foreign_sessions.mojom-webui.js';
import type {Device, PageCallbackRouter, QueryResult, QueryState, TimePeriodOption} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {TimePeriodOptionId} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import type {CrPageSelectorElement} from 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import {FindShortcutMixinLit} from 'chrome://resources/cr_elements/find_shortcut_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {hasKeyModifiers} from 'chrome://resources/js/util.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {browserProxyFactory as userEducationProxyFactory} from 'chrome://resources/mojo/components/user_education/webui/user_education.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {HistoryIdentityState} from './browser_proxy.js';
import {HistoryPageViewHistogram, HistorySignInState, SyncState} from './constants.js';
import type {HistoryListElement} from './history_list.js';
import type {HistoryToolbarElement} from './history_toolbar.js';
import {Page, TABBED_PAGES} from './router.js';
import type {HistoryRouterElement} from './router.js';
import type {FooterInfo, HistorySideBarElement} from './side_bar.js';

// Click/auxclick listeners to intercept any link clicks. If the link points
// to a chrome: or file: url, then calls into the browser to do the
// navigation.
function onDocumentClick(evt: Event) {
  const e = evt as MouseEvent;
  // Ignore buttons other than left and middle.
  if (e.button > 1 || e.defaultPrevented) {
    return;
  }

  const eventPath = e.composedPath() as HTMLElement[];
  let anchor: HTMLAnchorElement|null = null;
  if (eventPath) {
    for (const element of eventPath) {
      if (element.tagName === 'A' && (element as HTMLAnchorElement).href) {
        anchor = element as HTMLAnchorElement;
        break;
      }
    }
  }

  // Fallback if Event.path is not available.
  let el = e.target as HTMLElement;
  if (!anchor && el.nodeType === Node.ELEMENT_NODE &&
      el.webkitMatchesSelector('A, A *')) {
    while (el.tagName !== 'A') {
      el = el.parentElement as HTMLElement;
    }
    anchor = el as HTMLAnchorElement;
  }

  if (!anchor) {
    return;
  }

  if ((anchor.protocol === 'file:' || anchor.protocol === 'about:') &&
      (e.button === 0 || e.button === 1)) {
    BrowserProxyImpl.getInstance().navigateToUrl(anchor.href, anchor.target, e);
    e.preventDefault();
  }
}
export interface HistoryAppElement {
  $: {
    content: CrPageSelectorElement,
    contentSideBar: HistorySideBarElement,
    drawer: CrLazyRenderLitElement<CrDrawerElement>,
    history: HistoryListElement,
    tabsContainer: HTMLElement,
    tabsContent: CrPageSelectorElement,
    toolbar: HistoryToolbarElement,
    tabsScrollContainer: HTMLElement,
    router: HistoryRouterElement,
  };
}

const HistoryAppElementBase = HelpBubbleMixinLit(
    FindShortcutMixinLit(WebUiListenerMixinLit(CrLitElement)));

export class HistoryAppElement extends HistoryAppElementBase {
  static get is() {
    return 'history-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // <if expr="not is_chromeos">
      unoPhase2FollowUpEnabled_: {type: Boolean},
      shouldShowHistorySyncPromo_: {type: Boolean},
      shouldShowHistoryCrossDeviceSigninPromo_: {type: Boolean},
      // </if>
      contentPage_: {type: String},
      tabsContentPage_: {type: String},
      // The id of the currently selected page.
      selectedPage_: {type: String},
      queryResult_: {type: Object},
      sessionList_: {type: Array},
      // Updated on synced-device-manager attach by chrome.sending
      // 'otherDevicesInitialized'.
      identityState_: {type: Object},
      pendingDelete_: {type: Boolean},
      queryState_: {type: Object},
      // True if the window is narrow enough for the page to have a drawer.
      hasDrawer_: {type: Boolean},
      footerInfo: {type: Object},
      historyClustersEnabled_: {type: Boolean},
      historyClustersVisible_: {type: Boolean},
      lastSelectedTab_: {type: Number},
      showHistoryClusters_: {
        type: Boolean,
        reflect: true,
      },
      showTabs_: {type: Boolean},
      // The index of the currently selected tab.
      selectedTab_: {type: Number},
      tabsIcons_: {type: Array},
      tabsNames_: {type: Array},
      scrollTarget_: {type: Object},
      devices_: {type: Array},
      domain_: {type: String},
      includeActorVisits_: {type: Boolean},
      includeUserVisits_: {type: Boolean},
      clientIds_: {type: Array},
      isGlicWebActuationAvailable_: {type: Boolean},
      isBrowsingHistoryFilterByDeviceEnabled_: {type: Boolean},
      isBrowsingHistoryFilterByDomainEnabled_: {type: Boolean},
      isBrowsingHistoryFilterByTimePeriodEnabled_: {type: Boolean},
      timePeriodOptions_: {type: Array},
      selectedTimePeriodOptionId_: {type: Number},
    };
  }

  accessor footerInfo: FooterInfo = {
    managed: loadTimeData.getBoolean('isManaged'),
    otherFormsOfHistory: false,
    geminiAppsActivity: loadTimeData.getBoolean('isGlicEnabled'),
  };
  // <if expr="not is_chromeos">
  private accessor unoPhase2FollowUpEnabled_: boolean =
      loadTimeData.getBoolean('unoPhase2FollowUp');
  private accessor shouldShowHistorySyncPromo_: boolean = false;
  protected accessor shouldShowHistoryCrossDeviceSigninPromo_: boolean = false;
  // </if>
  protected accessor hasDrawer_: boolean = false;
  protected accessor historyClustersEnabled_: boolean =
      loadTimeData.getBoolean('isHistoryClustersEnabled');
  protected accessor historyClustersVisible_: boolean =
      loadTimeData.getBoolean('isHistoryClustersVisible');
  private accessor identityState_: HistoryIdentityState = {
    signIn: HistorySignInState.SIGNED_OUT,
    tabsSync: SyncState.TURNED_OFF,
    historySync: SyncState.TURNED_OFF,
  };
  protected accessor lastSelectedTab_: number =
      loadTimeData.getInteger('lastSelectedTab');
  protected accessor contentPage_: string = Page.HISTORY;
  protected accessor tabsContentPage_: string = Page.HISTORY;
  protected accessor pendingDelete_: boolean = false;
  protected accessor queryResult_: QueryResult = {
    info: null,
    value: [],
  };
  protected accessor sessionList_: ForeignSession[]|null = null;
  protected accessor queryState_: QueryState = {
    incremental: false,
    querying: false,
    searchTerm: '',
    after: null,
    includeUserVisits: true,
    includeActorVisits: true,
    clientIds: [],
    timePeriodOptionId: TimePeriodOptionId.kAllTime,
  };
  protected accessor selectedPage_: string = Page.HISTORY;
  protected accessor selectedTab_: number =
      loadTimeData.getInteger('lastSelectedTab') || 0;
  protected accessor showTabs_: boolean = false;
  protected accessor showHistoryClusters_: boolean = false;
  protected accessor tabsIcons_: string[] = [
    (document.documentElement.hasAttribute('webui-rounded-icons') ?
         'images/list.svg' :
         'images/list_old.svg'),
    (document.documentElement.hasAttribute('webui-rounded-icons') ?
         'chrome://resources/images/icon_journeys.svg' :
         'chrome://resources/images/icon_journeys_old.svg'),
  ];
  protected accessor tabsNames_: string[] = [
    loadTimeData.getString('historyListTabLabel'),
    loadTimeData.getString('historyClustersTabLabel'),
  ];
  protected accessor scrollTarget_: HTMLElement = document.body;
  protected accessor devices_: Device[] = [];
  protected accessor domain_: string = '';
  protected accessor includeActorVisits_: boolean = true;
  protected accessor includeUserVisits_: boolean = true;
  protected accessor clientIds_: string[] = [];
  private accessor isGlicWebActuationAvailable_: boolean =
      loadTimeData.getBoolean('isGlicWebActuationAvailable');
  private accessor isBrowsingHistoryFilterByDeviceEnabled_: boolean =
      loadTimeData.getBoolean('isBrowsingHistoryFilterByDeviceEnabled');
  private accessor isBrowsingHistoryFilterByDomainEnabled_: boolean =
      loadTimeData.getBoolean('isBrowsingHistoryFilterByDomainEnabled');
  private accessor isBrowsingHistoryFilterByTimePeriodEnabled_: boolean =
      loadTimeData.getBoolean('isBrowsingHistoryFilterByTimePeriodEnabled');
  protected accessor timePeriodOptions_: TimePeriodOption[] = [];
  protected accessor selectedTimePeriodOptionId_: TimePeriodOptionId =
      TimePeriodOptionId.kAllTime;

  private callbackRouter_: PageCallbackRouter;
  private foreignSessionCallbackRouter_: ForeignSessionPageCallbackRouter;
  private eventTracker_: EventTracker = new EventTracker();
  private historyClustersViewStartTime_: Date|null = null;
  private lastRecordedSelectedPageHistogramValue_: HistoryPageViewHistogram =
      HistoryPageViewHistogram.END;
  private onForeignSessionsChangedListenerId_: number|null = null;
  private onHasOtherFormsChangedListenerId_: number|null = null;

  constructor() {
    super();
    this.callbackRouter_ = BrowserProxyImpl.getInstance().callbackRouter;
    this.foreignSessionCallbackRouter_ =
        foreignSessionBrowserProxyFactory.getInstance().callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();

    // <if expr="is_android">
    this.addThemedColors_();
    ColorChangeUpdater.forDocument().start();
    // </if>

    // <if expr="not is_android">
    const enableWebuiRefresh2026 =
        loadTimeData.getString('webuiRefresh2026') !== '';
    if (enableWebuiRefresh2026) {
      this.addThemedColors_();
      ColorChangeUpdater.forDocument().start();
    }
    // </if>

    this.eventTracker_.add(document, 'click', onDocumentClick);
    this.eventTracker_.add(document, 'auxclick', onDocumentClick);
    this.eventTracker_.add(
        document, 'keydown', (e: Event) => this.onKeyDown_(e as KeyboardEvent));
    this.eventTracker_.add(
        document, 'visibilitychange', this.onVisibilityChange_.bind(this));
    this.eventTracker_.add(
        document, 'record-history-link-click',
        this.onRecordHistoryLinkClick_.bind(this));
    this.addWebUiListener(
        'history-identity-state-changed',
        (identityState: HistoryIdentityState) =>
            this.onIdentityStateChanged_(identityState));
    this.onForeignSessionsChangedListenerId_ =
        this.foreignSessionCallbackRouter_.onForeignSessionsChanged.addListener(
            (sessionList: ForeignSession[]) =>
                this.setForeignSessions_(sessionList));
    this.shadowRoot.querySelector('history-query-manager')!.initialize();
    foreignSessionBrowserProxyFactory.getInstance()
        .handler.getForeignSessions()
        .then(({sessions}) => this.setForeignSessions_(sessions));
    BrowserProxyImpl.getInstance().getInitialIdentityState().then(
        (identityState: HistoryIdentityState) =>
            this.onIdentityStateChanged_(identityState));
    BrowserProxyImpl.getInstance().getSyncedDevicesForFiltering().then(
        ({devices}) => {
          this.devices_ = devices;
        });
    BrowserProxyImpl.getInstance().getTimePeriodOptions().then(({options}) => {
      this.timePeriodOptions_ = options;
    });

    const mediaQuery = window.matchMedia('(max-width: 1023px)');
    this.hasDrawer_ = mediaQuery.matches;
    this.eventTracker_.add(
        mediaQuery, 'change',
        (e: MediaQueryListEvent) => this.hasDrawer_ = e.matches);

    this.onHasOtherFormsChangedListenerId_ =
        this.callbackRouter_.onHasOtherFormsChanged.addListener(
            (hasOtherForms: boolean) =>
                this.onHasOtherFormsChanged_(hasOtherForms));
    // <if expr="not is_chromeos">
    BrowserProxyImpl.getInstance()
        .handler.shouldShowHistoryPageHistorySyncPromo()
        .then(
            ({shouldShow}) =>
                this.handleShouldShowHistoryPageHistorySyncPromoChanged_(
                    shouldShow));
    // </if>
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();

    if (this.onHasOtherFormsChangedListenerId_ !== null) {
      this.callbackRouter_.removeListener(
          this.onHasOtherFormsChangedListenerId_);
      this.onHasOtherFormsChangedListenerId_ = null;
    }

    if (this.onForeignSessionsChangedListenerId_ !== null) {
      this.foreignSessionCallbackRouter_.removeListener(
          this.onForeignSessionsChangedListenerId_);
      this.onForeignSessionsChangedListenerId_ = null;
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('historyClustersEnabled_') ||
        changedPrivateProperties.has('historyClustersVisible_')) {
      this.showHistoryClusters_ =
          this.historyClustersEnabled_ && this.historyClustersVisible_;
    }

    if (changedPrivateProperties.has('showHistoryClusters_')) {
      this.showTabs_ = this.showHistoryClusters_;
    }

    if (changedPrivateProperties.has('selectedTab_')) {
      this.lastSelectedTab_ = this.selectedTab_;
      // Change in the currently selected tab requires change in the currently
      // selected page.
      if (!this.selectedPage_ || TABBED_PAGES.includes(this.selectedPage_)) {
        this.selectedPage_ = TABBED_PAGES[this.selectedTab_]!;
      }
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.addEventListener('cr-toolbar-menu-click', this.onCrToolbarMenuClick_);
    this.addEventListener('delete-selected', this.deleteSelected);
    this.addEventListener('open-selected', this.openSelected);
    this.addEventListener('history-checkbox-select', this.checkboxSelected);
    this.addEventListener('history-close-drawer', this.closeDrawer_);
    this.addEventListener('history-view-changed', this.historyViewChanged_);
    this.addEventListener('unselect-all', this.unselectAll);
  }

  getScrollTargetForTesting(): HTMLElement {
    return this.scrollTarget_;
  }

  protected getShowResultsByGroup_(): boolean {
    return this.selectedPage_ === Page.HISTORY_CLUSTERS;
  }

  protected getShowHistoryList_(): boolean {
    return this.selectedPage_ === Page.HISTORY;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('selectedTab_')) {
      BrowserProxyImpl.getInstance().handler.setLastSelectedTab(
          this.selectedTab_);
    }

    if (changedPrivateProperties.has('selectedPage_')) {
      this.selectedPageChanged_(
          changedPrivateProperties.get('selectedPage_') as string);
    }

    if (changedPrivateProperties.has('hasDrawer_')) {
      this.hasDrawerChanged_();
    }
  }

  protected historyClustersSelected_(): boolean {
    return this.selectedPage_ === Page.HISTORY_CLUSTERS &&
        this.showHistoryClusters_;
  }

  private onFirstRender_() {
    // Focus the search field on load. Done here to ensure the history page
    // is rendered before we try to take focus.
    const searchField = this.$.toolbar.searchField;
    if (!searchField.narrow) {
      searchField.getSearchInput().focus();
    }

    if (loadTimeData.getBoolean('isCriticalActionsEnabled') &&
        this.showFilterChips_()) {
      const filterChipsEl =
          this.shadowRoot.querySelector<CrLitElement>('#historyFilterChips');
      if (filterChipsEl) {
        filterChipsEl.updateComplete.then(() => {
          const chipEl = filterChipsEl.shadowRoot.querySelector<HTMLElement>(
              '#actorVisitsChip');
          if (chipEl) {
            this.registerHelpBubble(
                'HistoryUI::kHistoryGeminiFilterChipElementId', chipEl);
            setTimeout(() => {
              userEducationProxyFactory.getInstance()
                  .handler.maybeShowFeaturePromo(
                      {featureName: 'IPH_CriticalActionFilterChip', key: null});
            }, 1000);
          }
        });
      }
    }

    requestIdleCallback(function() {
      document.fonts.load('bold 12px Roboto');
    });
  }

  private onCrToolbarMenuClick_() {
    this.$.drawer.get().toggle();
  }

  /**
   * Listens for history-item being selected or deselected (through checkbox)
   * and changes the view of the top toolbar.
   */
  checkboxSelected() {
    this.$.toolbar.count = this.$.history.getSelectedItemCount();
  }

  selectOrUnselectAll() {
    this.$.history.selectOrUnselectAll();
    this.$.toolbar.count = this.$.history.getSelectedItemCount();
  }

  /**
   * Listens for call to cancel selection and loops through all items to set
   * checkbox to be unselected.
   */
  private unselectAll() {
    this.$.history.unselectAllItems();
    this.$.toolbar.count = 0;
  }

  deleteSelected() {
    this.$.history.deleteSelectedWithPrompt();
  }

  openSelected() {
    this.$.history.openSelected();
  }

  protected onQueryFinished_(e: CustomEvent<{result: QueryResult}>) {
    this.queryResult_ = e.detail.result;
    this.$.history.historyResult(e.detail.result.info!, e.detail.result.value);
    if (document.body.classList.contains('loading')) {
      document.body.classList.remove('loading');
      this.onFirstRender_();
      if (document.visibilityState === 'visible') {
        requestAnimationFrame(() => {
          BrowserProxyImpl.getInstance().recordTime(
              'HistoryPage.TimeToFirstVisibleContent',
              Math.round(performance.now()));
        });
      }
    }
  }

  private onKeyDown_(e: KeyboardEvent) {
    if ((e.key === 'Delete' || e.key === 'Backspace') && !hasKeyModifiers(e)) {
      this.onDeleteCommand_();
      return;
    }

    if (e.key === 'a' && !e.altKey && !e.shiftKey) {
      let hasTriggerModifier = e.ctrlKey && !e.metaKey;
      // <if expr="is_macosx">
      hasTriggerModifier = !e.ctrlKey && e.metaKey;
      // </if>
      if (hasTriggerModifier && this.onSelectAllCommand_()) {
        e.preventDefault();
      }
    }

    if (e.key === 'Escape') {
      this.unselectAll();
      getAnnouncerInstance().announce(
          loadTimeData.getString('itemsUnselected'));
      e.preventDefault();
    }
  }

  private onVisibilityChange_() {
    if (this.selectedPage_ !== Page.HISTORY_CLUSTERS) {
      return;
    }

    if (document.visibilityState === 'hidden') {
      this.recordHistoryClustersDuration_();
    } else if (
        document.visibilityState === 'visible' &&
        this.historyClustersViewStartTime_ === null) {
      // Restart the timer if the user switches back to the History tab.
      this.historyClustersViewStartTime_ = new Date();
    }
  }

  private onRecordHistoryLinkClick_(
      e: CustomEvent<{resultType: HistoryResultType, index: number}>) {
    // All of the above code only applies to History search results, not the
    // zero-query state. Check queryResult_ instead of queryState_ to key on
    // actually displayed results rather than the latest user input, which may
    // not have finished loading yet.
    if (!this.queryResult_.info || !this.queryResult_.info.term) {
      return;
    }

    const browserProxy = BrowserProxyImpl.getInstance();
    browserProxy.recordHistogram(
        'History.SearchResultClicked.Type', e.detail.resultType,
        HistoryResultType.END);

    // MetricsHandler uses a 100 bucket limit, so the max index is 99.
    const boundary = 100;
    const clampedIndex = Math.min(e.detail.index, 99);
    browserProxy.recordHistogram(
        'History.SearchResultClicked.Index', clampedIndex, boundary);

    switch (e.detail.resultType) {
      case HistoryResultType.TRADITIONAL:
        browserProxy.recordHistogram(
            'History.SearchResultClicked.Index.Traditional', clampedIndex,
            boundary);
        break;
      case HistoryResultType.GROUPED:
        browserProxy.recordHistogram(
            'History.SearchResultClicked.Index.Grouped', clampedIndex,
            boundary);
        break;
      case HistoryResultType.END:
        break;
      default:
        assertNotReachedCase(e.detail.resultType);
    }
  }

  private onDeleteCommand_() {
    if (this.$.toolbar.count === 0 || this.pendingDelete_) {
      return;
    }
    this.deleteSelected();
  }

  /**
   * @return Whether the command was actually triggered.
   */
  private onSelectAllCommand_(): boolean {
    if (this.$.toolbar.searchField.isSearchFocused() ||
        this.syncedTabsSelected_() || this.historyClustersSelected_()) {
      return false;
    }
    this.selectOrUnselectAll();
    return true;
  }

  /**
   * @param sessionList Array of objects describing the sessions from other
   *     devices.
   */
  private setForeignSessions_(sessionList: ForeignSession[]) {
    this.sessionList_ = sessionList;
  }

  /**
   * Updates the sign-in state.
   */
  private onIdentityStateChanged_(identityState: HistoryIdentityState) {
    this.identityState_ = identityState;
  }

  private onHasOtherFormsChanged_(hasOtherForms: boolean) {
    this.footerInfo = Object.assign(
        {}, this.footerInfo, {otherFormsOfHistory: hasOtherForms});
  }

  protected syncedTabsSelected_(): boolean {
    return this.selectedPage_ === Page.SYNCED_TABS;
  }

  /**
   * @return Whether a loading spinner should be shown (implies the
   *     backend is querying a new search term).
   */
  protected shouldShowSpinner_(): boolean {
    return this.queryState_.querying && !this.queryState_.incremental &&
        this.queryState_.searchTerm !== '';
  }

  private updateContentPage_() {
    switch (this.selectedPage_) {
      case Page.SYNCED_TABS:
        this.contentPage_ = Page.SYNCED_TABS;
        break;
      default:
        this.contentPage_ = Page.HISTORY;
    }
  }

  private updateTabsContentPage_() {
    this.tabsContentPage_ =
        (this.selectedPage_ === Page.HISTORY_CLUSTERS &&
         this.historyClustersEnabled_ && this.historyClustersVisible_) ?
        Page.HISTORY_CLUSTERS :
        Page.HISTORY;
  }

  private selectedPageChanged_(oldPage: string) {
    this.updateContentPage_();
    this.updateTabsContentPage_();
    this.unselectAll();
    this.historyViewChanged_();
    this.maybeUpdateSelectedHistoryTab_();

    if (oldPage === Page.HISTORY_CLUSTERS &&
        this.selectedPage_ !== Page.HISTORY_CLUSTERS) {
      this.recordHistoryClustersDuration_();
    }
    if (this.selectedPage_ === Page.HISTORY_CLUSTERS) {
      this.historyClustersViewStartTime_ = new Date();
    }
  }

  protected onContentIronSelect_() {
    this.updateScrollTarget_();
  }

  protected onTabsContentIronSelect_() {
    this.updateScrollTarget_();
  }

  private updateScrollTarget_() {
    const topLevelIronPages = this.$.content;
    const topLevelHistoryPage = this.$.tabsContainer;
    if (topLevelIronPages.selectedItem &&
        topLevelIronPages.selectedItem === topLevelHistoryPage) {
      this.scrollTarget_ = this.$.tabsScrollContainer;

      // Scroll target won't change as the scroll target for both Date and Group
      // view is this.$.tabsScrollContainer, which means history-list's
      // callbacks to fill the viewport do not get triggered automatically.
      this.$.history.fillCurrentViewport();
    } else {
      this.scrollTarget_ = topLevelIronPages.selectedItem as HTMLElement;
    }
  }

  private maybeUpdateSelectedHistoryTab_() {
    // Change in the currently selected page may require change in the currently
    // selected tab.
    if (TABBED_PAGES.includes(this.selectedPage_)) {
      this.selectedTab_ = TABBED_PAGES.indexOf(this.selectedPage_);
    }
  }

  private historyViewChanged_() {
    this.recordHistoryPageView_();
  }

  // Records the history clusters page duration.
  private recordHistoryClustersDuration_() {
    assert(this.historyClustersViewStartTime_ !== null);

    const duration =
        new Date().getTime() - this.historyClustersViewStartTime_.getTime();
    BrowserProxyImpl.getInstance().recordLongTime(
        'History.Clusters.WebUISessionDuration', duration);

    this.historyClustersViewStartTime_ = null;
  }

  private hasDrawerChanged_() {
    const drawer = this.$.drawer.getIfExists();
    if (!this.hasDrawer_ && drawer && drawer.open) {
      drawer.cancel();
    }
  }

  private closeDrawer_() {
    const drawer = this.$.drawer.get();
    if (drawer && drawer.open) {
      drawer.close();
    }
  }

  private recordHistoryPageView_() {
    let histogramValue = HistoryPageViewHistogram.END;
    switch (this.selectedPage_) {
      case Page.HISTORY_CLUSTERS:
        histogramValue = HistoryPageViewHistogram.JOURNEYS;
        break;
      case Page.SYNCED_TABS:
        histogramValue =
            this.identityState_.signIn === HistorySignInState.SIGNED_IN &&
                this.identityState_.tabsSync === SyncState.TURNED_ON ?
            HistoryPageViewHistogram.SYNCED_TABS :
            HistoryPageViewHistogram.SIGNIN_PROMO;
        break;
      default:
        histogramValue = HistoryPageViewHistogram.HISTORY;
        break;
    }

    // Avoid double-recording the same page consecutively.
    if (histogramValue === this.lastRecordedSelectedPageHistogramValue_) {
      return;
    }
    this.lastRecordedSelectedPageHistogramValue_ = histogramValue;

    BrowserProxyImpl.getInstance().recordHistogram(
        'History.HistoryPageView', histogramValue,
        HistoryPageViewHistogram.END);
  }

  // Override FindShortcutMixin methods.
  override handleFindShortcut(modalContextOpen: boolean): boolean {
    if (modalContextOpen) {
      return false;
    }
    this.$.toolbar.searchField.showAndFocus();
    return true;
  }

  // Override FindShortcutMixin methods.
  override searchInputHasFocus(): boolean {
    return this.$.toolbar.searchField.isSearchFocused();
  }

  setHasDrawerForTesting(enabled: boolean) {
    this.hasDrawer_ = enabled;
  }

  // <if expr="not is_chromeos">
  // History sync promo is shown based on the following conditions:
  // 1. UNO phase 2 follow up feature flag is enabled.
  // 2. Should be shown based on user prefs (the promo was closed < 5 times)
  // 3. History sync is not disabled.
  // 4. User is not already signed in and syncing history.
  protected shouldShowHistoryPageHistorySyncPromo_(): boolean {
    return this.unoPhase2FollowUpEnabled_ && this.shouldShowHistorySyncPromo_ &&
        this.identityState_.historySync !== SyncState.DISABLED &&
        !(this.identityState_.signIn === HistorySignInState.SIGNED_IN &&
          this.identityState_.historySync === SyncState.TURNED_ON);
  }

  private handleShouldShowHistoryPageHistorySyncPromoChanged_(
      shouldShowHistorySyncPromo: boolean) {
    this.shouldShowHistorySyncPromo_ = shouldShowHistorySyncPromo;
  }

  protected onShouldShowHistoryCrossDeviceSigninPromo_(
      e: CustomEvent<{shouldShow: boolean}>) {
    this.shouldShowHistoryCrossDeviceSigninPromo_ = e.detail.shouldShow;
  }
  // </if>

  protected onQueryStateChanged_(e: CustomEvent<{value: QueryState}>) {
    this.queryState_ = e.detail.value;
  }

  protected onSelectedPageChanged_(e: CustomEvent<{value: string}>) {
    this.selectedPage_ = e.detail.value;
  }

  protected onListPendingDeleteChanged_(e: CustomEvent<{value: boolean}>) {
    this.pendingDelete_ = e.detail.value;
  }

  protected onTabsSelectedChanged_(e: CustomEvent<{value: number}>) {
    this.onSelectedTabChanged_(e);
  }

  protected onSelectedTabChanged_(e: CustomEvent<{value: number}>) {
    this.selectedTab_ = e.detail.value;
  }

  protected onHistoryClustersVisibleChanged_(e: CustomEvent<{value: boolean}>) {
    this.historyClustersVisible_ = e.detail.value;
  }

  protected onDomainFilterSelected_(e: CustomEvent<{domain: string}>) {
    this.domain_ = e.detail.domain;
  }

  protected onFilterChanged_(e: CustomEvent<{
    userVisits: boolean,
    actorVisits: boolean,
    clientIds: string[],
    timePeriodOptionId: TimePeriodOptionId,
  }>) {
    this.includeUserVisits_ = e.detail.userVisits;
    this.includeActorVisits_ = e.detail.actorVisits;
    this.clientIds_ = e.detail.clientIds;
    this.selectedTimePeriodOptionId_ = e.detail.timePeriodOptionId;

    this.fire('change-query', {
      search: this.queryState_.searchTerm,
      includeActorVisits: this.includeActorVisits_,
      includeUserVisits: this.includeUserVisits_,
      clientIds: this.clientIds_,
      timePeriodOptionId: this.selectedTimePeriodOptionId_,
    });
  }

  protected showActorFilter_(): boolean {
    return this.isGlicWebActuationAvailable_;
  }

  protected showDeviceFilter_(): boolean {
    return this.isBrowsingHistoryFilterByDeviceEnabled_ &&
        this.devices_.length > 1;
  }

  protected showDomainFilter_(): boolean {
    return this.isBrowsingHistoryFilterByDomainEnabled_ && this.domain_ !== '';
  }

  protected showTimePeriodFilter_(): boolean {
    return this.isBrowsingHistoryFilterByTimePeriodEnabled_ &&
        this.timePeriodOptions_.length > 0;
  }

  protected showFilterChips_(): boolean {
    return (this.showActorFilter_() || this.showDeviceFilter_() ||
            this.showDomainFilter_() || this.showTimePeriodFilter_()) &&
        !this.getShowResultsByGroup_();
  }

  // TODO(crub.com/509908129): Add static stylesheet in history.html
  private addThemedColors_() {
    assert(document.body.querySelector(COLORS_CSS_SELECTOR) === null);
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = 'chrome://theme/colors.css?sets=ui,chrome';
    document.body.appendChild(link);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-app': HistoryAppElement;
  }
}

customElements.define(HistoryAppElement.is, HistoryAppElement);
