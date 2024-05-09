// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/history_clusters/clusters.js';
import 'chrome://resources/cr_components/history_embeddings/filter_chips.js';
import 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './history_embeddings_promo.js';
import './history_list.js';
import './history_toolbar.js';
import './query_manager.js';
import './shared_style.css.js';
import './side_bar.js';
import './strings.m.js';

import {HistoryResultType} from 'chrome://resources/cr_components/history/constants.js';
import type {Suggestion} from 'chrome://resources/cr_components/history_embeddings/filter_chips.js';
import type {HistoryEmbeddingsMoreActionsClickEvent} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import type {FindShortcutMixinInterface} from 'chrome://resources/cr_elements/find_shortcut_mixin.js';
import {FindShortcutMixin} from 'chrome://resources/cr_elements/find_shortcut_mixin.js';
import type {WebUiListenerMixinInterface} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getTrustedScriptURL} from 'chrome://resources/js/static_types.js';
import {hasKeyModifiers} from 'chrome://resources/js/util.js';
import type {IronPagesElement} from 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import {IronScrollTargetBehavior} from 'chrome://resources/polymer/v3_0/iron-scroll-target-behavior/iron-scroll-target-behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import type {BrowserService} from './browser_service.js';
import {BrowserServiceImpl} from './browser_service.js';
import {HistoryPageViewHistogram} from './constants.js';
import type {ForeignSession, QueryResult, QueryState} from './externs.js';
import type {HistoryListElement} from './history_list.js';
import type {HistoryToolbarElement} from './history_toolbar.js';
import {convertDateToQueryValue} from './query_manager.js';
import {Page, TABBED_PAGES} from './router.js';
import type {HistoryRouterElement} from './router.js';
import type {FooterInfo, HistorySideBarElement} from './side_bar.js';

let lazyLoadPromise: Promise<void>|null = null;
export function ensureLazyLoaded(): Promise<void> {
  if (!lazyLoadPromise) {
    const script = document.createElement('script');
    script.type = 'module';
    script.src = getTrustedScriptURL`./lazy_load.js` as unknown as string;
    document.body.appendChild(script);

    lazyLoadPromise = Promise.all([
      customElements.whenDefined('history-synced-device-manager'),
      customElements.whenDefined('product-specifications-lists'),
      customElements.whenDefined('cr-action-menu'),
      customElements.whenDefined('cr-button'),
      customElements.whenDefined('cr-checkbox'),
      customElements.whenDefined('cr-dialog'),
      customElements.whenDefined('cr-drawer'),
      customElements.whenDefined('cr-icon-button'),
      customElements.whenDefined('cr-toolbar-selection-overlay'),
    ]) as unknown as Promise<void>;
  }
  return lazyLoadPromise;
}

// Adds click/auxclick listeners for any link on the page. If the link points
// to a chrome: or file: url, then calls into the browser to do the
// navigation. Note: This method is *not* re-entrant. Every call to it, will
// re-add listeners on |document|. It's up to callers to ensure this is only
// called once.
export function listenForPrivilegedLinkClicks() {
  ['click', 'auxclick'].forEach(function(eventName) {
    document.addEventListener(eventName, function(evt: Event) {
      const e = evt as MouseEvent;
      // Ignore buttons other than left and middle.
      if (e.button > 1 || e.defaultPrevented) {
        return;
      }

      const eventPath = e.composedPath() as HTMLElement[];
      let anchor: HTMLAnchorElement|null = null;
      if (eventPath) {
        for (let i = 0; i < eventPath.length; i++) {
          const element = eventPath[i];
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
        BrowserServiceImpl.getInstance().navigateToUrl(
            anchor.href, anchor.target, e);
        e.preventDefault();
      }
    });
  });
}

export interface HistoryAppElement {
  $: {
    'content': IronPagesElement,
    'content-side-bar': HistorySideBarElement,
    'drawer': CrLazyRenderElement<CrDrawerElement>,
    'history': HistoryListElement,
    'tabs-container': Element,
    'tabs-content': IronPagesElement,
    'toolbar': HistoryToolbarElement,
    tabsScrollContainer: HTMLElement,
    router: HistoryRouterElement,
    historyEmbeddingsContainer: HTMLElement,
  };
}

const HistoryAppElementBase =
    mixinBehaviors(
        [IronScrollTargetBehavior],
        FindShortcutMixin(WebUiListenerMixin(PolymerElement))) as {
      new (): PolymerElement & FindShortcutMixinInterface &
          IronScrollTargetBehavior & WebUiListenerMixinInterface,
    };

export class HistoryAppElement extends HistoryAppElementBase {
  static get is() {
    return 'history-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      enableHistoryEmbeddings_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableHistoryEmbeddings'),
      },

      // The id of the currently selected page.
      selectedPage_: {
        type: String,
        observer: 'selectedPageChanged_',
      },

      queryResult_: Object,

      // Updated on synced-device-manager attach by chrome.sending
      // 'otherDevicesInitialized'.
      isUserSignedIn_: Boolean,

      pendingDelete_: Boolean,

      toolbarShadow_: {
        type: Boolean,
        reflectToAttribute: true,
        notify: true,
      },

      queryState_: Object,

      // True if the window is narrow enough for the page to have a drawer.
      hasDrawer_: {
        type: Boolean,
        observer: 'hasDrawerChanged_',
      },

      footerInfo: {
        type: Object,
        value() {
          return {
            managed: loadTimeData.getBoolean('isManaged'),
            otherFormsOfHistory: false,
          };
        },
      },

      historyClustersEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isHistoryClustersEnabled'),
      },

      historyClustersVisible_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isHistoryClustersVisible'),
      },

      lastSelectedTab_: {
        type: Number,
        value: () => loadTimeData.getInteger('lastSelectedTab'),
      },

      showHistoryClusters_: {
        type: Boolean,
        computed:
            'computeShowHistoryClusters_(historyClustersEnabled_, historyClustersVisible_)',
        reflectToAttribute: true,
      },

      showTabs_: {
        type: Boolean,
        computed:
            'computeShowTabs_(showHistoryClusters_, enableHistoryEmbeddings_)',
      },

      // The index of the currently selected tab.
      selectedTab_: {
        type: Number,
        observer: 'selectedTabChanged_',
      },

      tabsIcons_: {
        type: Array,
        value: () =>
            ['images/list.svg', 'chrome://resources/images/icon_journeys.svg'],
      },

      tabsNames_: {
        type: Array,
        value: () => {
          return [
            loadTimeData.getString('historyListTabLabel'),
            loadTimeData.getString('historyClustersTabLabel'),
          ];
        },
      },

      scrollTarget_: Object,

      queryStateAfterDate_: {
        type: Object,
        computed: 'computeQueryStateAfterDate_(queryState_.*)',
      },

      hasHistoryEmbeddingsResults_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      productSpecificationsListsEnabled_: Boolean,
      tabContentScrollOffset_: Number,
    };
  }

  footerInfo: FooterInfo;
  private browserService_: BrowserService = BrowserServiceImpl.getInstance();
  private enableHistoryEmbeddings_: boolean;
  private eventTracker_: EventTracker = new EventTracker();
  private hasDrawer_: boolean;
  private historyClustersEnabled_: boolean;
  private historyClustersVisible_: boolean;
  private isUserSignedIn_: boolean = loadTimeData.getBoolean('isUserSignedIn');
  private lastSelectedTab_: number;
  private pendingDelete_: boolean;
  private queryResult_: QueryResult;
  private queryState_: QueryState;
  private selectedPage_: string;
  private selectedTab_: number;
  private lastRecordedSelectedPageHistogramValue_: HistoryPageViewHistogram =
      HistoryPageViewHistogram.END;
  private showHistoryClusters_: boolean;
  private tabsIcons_: string[];
  private tabsNames_: string[];
  private toolbarShadow_: boolean;
  private historyClustersViewStartTime_: Date|null = null;
  private scrollTarget_: HTMLElement;
  private queryStateAfterDate_?: Date;
  private hasHistoryEmbeddingsResults_: boolean;
  private productSpecificationsListsEnabled_: boolean =
      loadTimeData.getBoolean('productSpecificationsListsEnabled');
  private historyEmbeddingsResizeObserver_?: ResizeObserver;
  private tabContentScrollOffset_: number = 0;

  constructor() {
    super();

    this.queryResult_ = {
      info: undefined,
      results: undefined,
      sessionList: undefined,
    };

    listenForPrivilegedLinkClicks();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        document, 'keydown', (e: Event) => this.onKeyDown_(e as KeyboardEvent));
    this.eventTracker_.add(
        document, 'visibilitychange', this.onVisibilityChange_.bind(this));
    this.eventTracker_.add(
        document, 'record-history-link-click',
        this.onRecordHistoryLinkClick_.bind(this));
    this.addWebUiListener(
        'sign-in-state-changed',
        (signedIn: boolean) => this.onSignInStateChanged_(signedIn));
    this.addWebUiListener(
        'has-other-forms-changed',
        (hasOtherForms: boolean) =>
            this.onHasOtherFormsChanged_(hasOtherForms));
    this.addWebUiListener(
        'foreign-sessions-changed',
        (sessionList: ForeignSession[]) =>
            this.setForeignSessions_(sessionList));
    this.shadowRoot!.querySelector('history-query-manager')!.initialize();
    this.browserService_!.getForeignSessions().then(
        sessionList => this.setForeignSessions_(sessionList));
  }

  override ready() {
    super.ready();

    this.addEventListener('cr-toolbar-menu-click', this.onCrToolbarMenuClick_);
    this.addEventListener('delete-selected', this.deleteSelected);
    this.addEventListener('history-checkbox-select', this.checkboxSelected);
    this.addEventListener('history-close-drawer', this.closeDrawer_);
    this.addEventListener('history-view-changed', this.historyViewChanged_);
    this.addEventListener('unselect-all', this.unselectAll);
  }

  private getShowResultsByGroup_() {
    return this.selectedPage_ === Page.HISTORY_CLUSTERS;
  }

  private onShowResultsByGroupChanged_(e: CustomEvent<{value: boolean}>) {
    const showResultsByGroup = e.detail.value;
    if (showResultsByGroup) {
      this.selectedTab_ = TABBED_PAGES.indexOf(Page.HISTORY_CLUSTERS);
    } else {
      this.selectedTab_ = TABBED_PAGES.indexOf(Page.HISTORY);
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
    if (this.historyEmbeddingsResizeObserver_) {
      this.historyEmbeddingsResizeObserver_.disconnect();
      this.historyEmbeddingsResizeObserver_ = undefined;
    }
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private computeShowHistoryClusters_(): boolean {
    return this.historyClustersEnabled_ && this.historyClustersVisible_;
  }

  private computeShowTabs_(): boolean {
    return this.showHistoryClusters_ && !this.enableHistoryEmbeddings_;
  }

  private historyClustersSelected_(
      _selectedPage: string, _showHistoryClusters: boolean): boolean {
    return this.selectedPage_ === Page.HISTORY_CLUSTERS &&
        this.showHistoryClusters_;
  }

  private productSpecificationsListsSelected_(_selectedPage: string): boolean {
    return this.productSpecificationsListsEnabled_ &&
        this.selectedPage_ === Page.PRODUCT_SPECIFICATIONS_LISTS;
  }

  private onFirstRender_() {
    // Focus the search field on load. Done here to ensure the history page
    // is rendered before we try to take focus.
    const searchField = this.$.toolbar.searchField;
    if (!searchField.narrow) {
      searchField.getSearchInput().focus();
    }

    // Lazily load the remainder of the UI.
    ensureLazyLoaded().then(function() {
      requestIdleCallback(function() {
        // https://github.com/microsoft/TypeScript/issues/13569
        (document as any).fonts.load('bold 12px Roboto');
      });
    });
  }

  /** Overridden from IronScrollTargetBehavior */
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _scrollHandler() {
    if (this.scrollTarget) {
      // When the tabs are visible, show the toolbar shadow for the synced
      // devices page only.
      this.toolbarShadow_ = this.scrollTarget.scrollTop !== 0 &&
          (!this.showHistoryClusters_ ||
           this.syncedTabsSelected_(this.selectedPage_!));
    }
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

  private onQueryFinished_() {
    this.$.history.historyResult(
        this.queryResult_.info!, this.queryResult_.results!);
    if (document.body.classList.contains('loading')) {
      document.body.classList.remove('loading');
      this.onFirstRender_();
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

    this.browserService_!.recordHistogram(
        'History.SearchResultClicked.Type', e.detail.resultType,
        HistoryResultType.END);

    // MetricsHandler uses a 100 bucket limit, so the max index is 99.
    const maxIndex = 99;
    const clampedIndex = Math.min(e.detail.index, 99);
    this.browserService_!.recordHistogram(
        'History.SearchResultClicked.Index', clampedIndex, maxIndex);

    switch (e.detail.resultType) {
      case HistoryResultType.TRADITIONAL: {
        this.browserService_!.recordHistogram(
            'History.SearchResultClicked.Index.Traditional', clampedIndex,
            maxIndex);
        break;
      }
      case HistoryResultType.GROUPED: {
        this.browserService_!.recordHistogram(
            'History.SearchResultClicked.Index.Grouped', clampedIndex,
            maxIndex);
        break;
      }
      case HistoryResultType.EMBEDDINGS: {
        this.browserService_!.recordHistogram(
            'History.SearchResultClicked.Index.Embeddings', clampedIndex,
            maxIndex);
        break;
      }
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
        this.syncedTabsSelected_(this.selectedPage_!) ||
        this.historyClustersSelected_(
            this.selectedPage_!, this.showHistoryClusters_)) {
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
    this.set('queryResult_.sessionList', sessionList);
  }

  /**
   * Update sign in state of synced device manager after user logs in or out.
   */
  private onSignInStateChanged_(isUserSignedIn: boolean) {
    this.isUserSignedIn_ = isUserSignedIn;
  }

  /**
   * Update sign in state of synced device manager after user logs in or out.
   */
  private onHasOtherFormsChanged_(hasOtherForms: boolean) {
    this.set('footerInfo.otherFormsOfHistory', hasOtherForms);
  }

  private syncedTabsSelected_(_selectedPage: string): boolean {
    return this.selectedPage_ === Page.SYNCED_TABS;
  }

  /**
   * @return Whether a loading spinner should be shown (implies the
   *     backend is querying a new search term).
   */
  private shouldShowSpinner_(
      querying: boolean, incremental: boolean, searchTerm: string): boolean {
    return querying && !incremental && searchTerm !== '';
  }

  private selectedPageChanged_(newPage: string, oldPage: string) {
    this.unselectAll();
    this.historyViewChanged_();
    this.maybeUpdateSelectedHistoryTab_();

    if (oldPage === Page.HISTORY_CLUSTERS &&
        newPage !== Page.HISTORY_CLUSTERS) {
      this.recordHistoryClustersDuration_();
    }
    if (newPage === Page.HISTORY_CLUSTERS) {
      this.historyClustersViewStartTime_ = new Date();
    }
  }

  private updateScrollTarget_() {
    const topLevelIronPages = this.$['content'];
    const topLevelHistoryPage = this.$['tabs-container'];
    if (topLevelIronPages.selectedItem &&
        topLevelIronPages.selectedItem === topLevelHistoryPage) {
      // The top-level History page has another inner IronPages element that
      // can toggle between different pages.
      this.scrollTarget = this.$.tabsScrollContainer;
    } else if (topLevelIronPages.selectedItem) {
      this.scrollTarget = topLevelIronPages.selectedItem as HTMLElement;
    } else {
      this.scrollTarget = null;
    }
  }

  private selectedTabChanged_() {
    this.lastSelectedTab_ = this.selectedTab_;
    // Change in the currently selected tab requires change in the currently
    // selected page.
    this.selectedPage_ = TABBED_PAGES[this.selectedTab_];
    this.browserService_!.setLastSelectedTab(this.selectedTab_);
  }

  private maybeUpdateSelectedHistoryTab_() {
    // Change in the currently selected page may require change in the currently
    // selected tab.
    if (TABBED_PAGES.includes(this.selectedPage_)) {
      this.selectedTab_ = TABBED_PAGES.indexOf(this.selectedPage_);
    }
  }

  private historyViewChanged_() {
    // This allows the synced-device-manager to render so that it can be set
    // as the scroll target.
    requestAnimationFrame(() => {
      this._scrollHandler();
    });
    this.recordHistoryPageView_();
  }

  // Records the history clusters page duration.
  private recordHistoryClustersDuration_() {
    assert(this.historyClustersViewStartTime_ !== null);

    const duration =
        new Date().getTime() - this.historyClustersViewStartTime_.getTime();
    this.browserService_!.recordLongTime(
        'History.Clusters.WebUISessionDuration', duration);

    this.historyClustersViewStartTime_ = null;
  }

  private hasDrawerChanged_() {
    const drawer = this.$.drawer.getIfExists();
    if (!this.hasDrawer_ && drawer && drawer.open) {
      drawer.cancel();
    }
  }

  /**
   * This computed binding is needed to make the iron-pages selector update
   * when <synced-device-manager> or <history-clusters> is instantiated for the
   * first time. Otherwise the fallback selection will continue to be used after
   * the corresponding item is added as a child of iron-pages.
   */
  private getSelectedPage_(selectedPage: string, _items: any[]): string {
    return selectedPage;
  }

  private closeDrawer_() {
    const drawer = this.$.drawer.get() as CrDrawerElement;
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
        histogramValue = this.isUserSignedIn_ ?
            HistoryPageViewHistogram.SYNCED_TABS :
            HistoryPageViewHistogram.SIGNIN_PROMO;
        break;
      case Page.PRODUCT_SPECIFICATIONS_LISTS:
        histogramValue = HistoryPageViewHistogram.PRODUCT_SPECIFICATIONS_LISTS;
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

    this.browserService_!.recordHistogram(
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

  private shouldShowHistoryEmbeddings_(): boolean {
    if (!loadTimeData.getBoolean('enableHistoryEmbeddings')) {
      return false;
    }

    if (!this.queryState_.searchTerm) {
      return false;
    }

    return this.queryState_.searchTerm.split(' ')
               .filter(part => part.length > 0)
               .length > 1;
  }

  private onSelectedSuggestionChanged_(e: CustomEvent<{value: Suggestion}>) {
    let afterString: string|undefined = undefined;
    if (e.detail.value?.timeRangeStart) {
      afterString = convertDateToQueryValue(e.detail.value.timeRangeStart);
    }

    this.fire_('change-query', {
      search: this.queryState_.searchTerm,
      after: afterString,
    });
  }

  private computeQueryStateAfterDate_(): Date|undefined {
    const afterString = this.queryState_.after;
    if (!afterString) {
      return undefined;
    }

    const afterDate = new Date(afterString + 'T00:00:00');

    // This compute function listens for any subproperty changes on the
    // queryState_ so the `after` param may not have changed.
    if (this.queryStateAfterDate_?.getTime() === afterDate.getTime()) {
      return this.queryStateAfterDate_;
    }

    return afterDate;
  }

  private onHistoryEmbeddingsItemMoreFromSiteClick_(
      e: HistoryEmbeddingsMoreActionsClickEvent) {
    const historyEmbeddingsItem = e.detail;
    this.fire_(
        'change-query',
        {search: 'host:' + new URL(historyEmbeddingsItem.url.url).hostname});
  }

  private onHistoryEmbeddingsItemRemoveClick_(
      e: HistoryEmbeddingsMoreActionsClickEvent) {
    const historyEmbeddingsItem = e.detail;
    this.browserService_.removeVisits([{
      url: historyEmbeddingsItem.url.url,
      timestamps: [historyEmbeddingsItem.lastUrlVisitTimestamp],
    }]);
  }

  private onHistoryEmbeddingsIsEmptyChanged_(e: CustomEvent<{value: boolean}>) {
    this.hasHistoryEmbeddingsResults_ = !e.detail.value;
  }

  private onHistoryEmbeddingsContainerShown_() {
    assert(this.enableHistoryEmbeddings_);
    const historyEmbeddingsContainer =
        this.shadowRoot!.querySelector('#historyEmbeddingsContainer');
    assert(historyEmbeddingsContainer);
    this.historyEmbeddingsResizeObserver_ = new ResizeObserver((entries) => {
      assert(entries.length === 1);
      this.tabContentScrollOffset_ = entries[0].contentRect.height;
    });
    this.historyEmbeddingsResizeObserver_.observe(historyEmbeddingsContainer);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-app': HistoryAppElement;
  }
}

customElements.define(HistoryAppElement.is, HistoryAppElement);
