// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';

import './history_clusters/clusters.js';
import './history_list.js';
import './history_toolbar.js';
import './query_manager.js';
import './router.js';
import './shared_style.js';
import './side_bar.js';
import './strings.m.js';

import {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {FindShortcutBehavior} from 'chrome://resources/cr_elements/find_shortcut_behavior.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {hasKeyModifiers} from 'chrome://resources/js/util.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {IronScrollTargetBehavior} from 'chrome://resources/polymer/v3_0/iron-scroll-target-behavior/iron-scroll-target-behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserService} from './browser_service.js';
import {HistoryPageViewHistogram} from './constants.js';
import {ForeignSession, QueryResult, QueryState} from './externs.js';
import {HistoryListElement} from './history_list.js';
import {HistoryToolbarElement} from './history_toolbar.js';
import {HistoryQueryManagerElement} from './query_manager.js';
import {FooterInfo} from './side_bar.js';

let lazyLoadPromise: Promise<void>|null = null;
export function ensureLazyLoaded(): Promise<void> {
  if (!lazyLoadPromise) {
    const script = document.createElement('script');
    script.type = 'module';
    script.src = './lazy_load.js';
    document.body.appendChild(script);

    lazyLoadPromise = Promise.all([
      customElements.whenDefined('history-synced-device-manager'),
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

      const eventPath = e.composedPath() as Array<HTMLElement>;
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
        BrowserService.getInstance().navigateToUrl(
            anchor.href, anchor.target, e);
        e.preventDefault();
      }
    });
  });
}

declare global {
  interface Window {
    // https://github.com/microsoft/TypeScript/issues/40807
    requestIdleCallback(callback: () => void): void;
  }
}

export interface HistoryAppElement {
  $: {
    'drawer': CrLazyRenderElement<CrDrawerElement>,
    'history': HistoryListElement,
    'toolbar': HistoryToolbarElement,
  };
}

const HistoryAppElementBase =
    mixinBehaviors(
        [FindShortcutBehavior, IronScrollTargetBehavior, WebUIListenerBehavior],
        PolymerElement) as {
      new (): PolymerElement & FindShortcutBehavior & IronScrollTargetBehavior &
      WebUIListenerBehavior
    };

export class HistoryAppElement extends HistoryAppElementBase {
  static get is() {
    return 'history-app';
  }

  static get properties() {
    return {
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

      /** @type {!QueryState} */
      queryState_: Object,

      // True if the window is narrow enough for the page to have a drawer.
      hasDrawer_: {
        type: Boolean,
        observer: 'hasDrawerChanged_',
      },

      /** @type {FooterInfo} */
      footerInfo: {
        type: Object,
        value() {
          return {
            managed: loadTimeData.getBoolean('isManaged'),
            otherFormsOfHistory: false,
          };
        },
      },
    };
  }

  private eventTracker_: EventTracker = new EventTracker();
  private browserService_: BrowserService|null = null;
  private queryResult_: QueryResult;
  private isUserSignedIn_: boolean = loadTimeData.getBoolean('isUserSignedIn');
  private pendingDelete_: boolean;
  private hasDrawer_: boolean;
  private selectedPage_: string;
  private toolbarShadow_: boolean;

  constructor() {
    super();

    this.queryResult_ = {
      info: undefined,
      results: undefined,
      sessionList: undefined,
    };

    listenForPrivilegedLinkClicks();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.eventTracker_.add(
        document, 'keydown', e => this.onKeyDown_(e as KeyboardEvent));
    this.addWebUIListener(
        'sign-in-state-changed',
        (signedIn: boolean) => this.onSignInStateChanged_(signedIn));
    this.addWebUIListener(
        'has-other-forms-changed',
        (hasOtherForms: boolean) =>
            this.onHasOtherFormsChanged_(hasOtherForms));
    this.addWebUIListener(
        'foreign-sessions-changed',
        (sessionList: Array<ForeignSession>) =>
            this.setForeignSessions_(sessionList));
    this.browserService_ = BrowserService.getInstance();
    this.shadowRoot!.querySelector('history-query-manager')!.initialize();
    this.browserService_!.getForeignSessions().then(
        sessionList => this.setForeignSessions_(sessionList));
  }

  ready() {
    super.ready();

    this.addEventListener('cr-toolbar-menu-tap', this.onCrToolbarMenuTap_);
    this.addEventListener('delete-selected', this.deleteSelected);
    this.addEventListener('history-checkbox-select', this.checkboxSelected);
    this.addEventListener('history-close-drawer', this.closeDrawer_);
    this.addEventListener('history-view-changed', this.historyViewChanged_);
    this.addEventListener('unselect-all', this.unselectAll);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private historyClustersSelected_(selectedPage: string): boolean {
    return selectedPage === 'journeys' &&
        loadTimeData.getBoolean('isHistoryClustersEnabled');
  }

  private onFirstRender_() {
    setTimeout(() => {
      this.browserService_!.recordTime(
          'History.ResultsRenderedTime', window.performance.now());
    });

    // Focus the search field on load. Done here to ensure the history page
    // is rendered before we try to take focus.
    const searchField =
        /** @type {HistoryToolbarElement} */ (this.$.toolbar).searchField;
    if (!searchField.narrow) {
      searchField.getSearchInput().focus();
    }

    // Lazily load the remainder of the UI.
    ensureLazyLoaded().then(function() {
      window.requestIdleCallback(function() {
        // https://github.com/microsoft/TypeScript/issues/13569
        (document as any).fonts.load('bold 12px Roboto');
      });
    });
  }

  /** Overridden from IronScrollTargetBehavior */
  _scrollHandler() {
    if (this.scrollTarget) {
      this.toolbarShadow_ = this.scrollTarget.scrollTop !== 0;
    }
  }

  private onCrToolbarMenuTap_() {
    this.$.drawer.get().toggle();
  }

  /**
   * Listens for history-item being selected or deselected (through checkbox)
   * and changes the view of the top toolbar.
   */
  checkboxSelected() {
    const toolbar = /** @type {HistoryToolbarElement} */ (this.$.toolbar);
    toolbar.count = /** @type {HistoryListElement} */ (this.$.history)
                        .getSelectedItemCount();
  }

  selectOrUnselectAll() {
    const list = /** @type {HistoryListElement} */ (this.$.history);
    const toolbar = /** @type {HistoryToolbarElement} */ (this.$.toolbar);
    list.selectOrUnselectAll();
    toolbar.count = list.getSelectedItemCount();
  }

  /**
   * Listens for call to cancel selection and loops through all items to set
   * checkbox to be unselected.
   */
  private unselectAll() {
    const list = /** @type {HistoryListElement} */ (this.$.history);
    const toolbar = /** @type {HistoryToolbarElement} */ (this.$.toolbar);
    list.unselectAllItems();
    toolbar.count = 0;
  }

  deleteSelected() {
    this.$.history.deleteSelectedWithPrompt();
  }

  private onQueryFinished_() {
    const list = /** @type {HistoryListElement} */ (this.$['history']);
    list.historyResult(this.queryResult_.info!, this.queryResult_.results!);
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
      IronA11yAnnouncer.requestAvailability();
      this.fire_(
          'iron-announce', {text: loadTimeData.getString('itemsUnselected')});
      e.preventDefault();
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
        this.historyClustersSelected_(this.selectedPage_!)) {
      return false;
    }
    this.selectOrUnselectAll();
    return true;
  }

  /**
   * @param sessionList Array of objects describing the sessions from other
   *     devices.
   */
  private setForeignSessions_(sessionList: Array<ForeignSession>) {
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

  private syncedTabsSelected_(selectedPage: string): boolean {
    return selectedPage === 'syncedTabs';
  }

  /**
   * @return Whether a loading spinner should be shown (implies the
   *     backend is querying a new search term).
   */
  private shouldShowSpinner_(
      querying: boolean, incremental: boolean, searchTerm: string): boolean {
    return querying && !incremental && searchTerm !== '';
  }

  private selectedPageChanged_() {
    this.unselectAll();
    this.historyViewChanged_();
  }

  private historyViewChanged_() {
    // This allows the synced-device-manager to render so that it can be set
    // as the scroll target.
    requestAnimationFrame(() => {
      this._scrollHandler();
    });
    this.recordHistoryPageView_();
  }

  private hasDrawerChanged_() {
    const drawer = this.$.drawer.getIfExists();
    if (!this.hasDrawer_ && drawer && drawer.open) {
      drawer.cancel();
    }
  }

  /**
   * This computed binding is needed to make the iron-pages selector update
   * when the synced-device-manager is instantiated for the first time.
   * Otherwise the fallback selection will continue to be used after the
   * corresponding item is added as a child of iron-pages.
   */
  private getSelectedPage_(selectedPage: string, _items: Array<any>): string {
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
      case 'syncedTabs':
        histogramValue = this.isUserSignedIn_ ?
            HistoryPageViewHistogram.SYNCED_TABS :
            HistoryPageViewHistogram.SIGNIN_PROMO;
        break;
      default:
        histogramValue = HistoryPageViewHistogram.HISTORY;
        break;
    }

    this.browserService_!.recordHistogram(
        'History.HistoryPageView', histogramValue,
        HistoryPageViewHistogram.END);
  }

  // Override FindShortcutBehavior methods.
  handleFindShortcut(modalContextOpen: boolean): boolean {
    if (modalContextOpen) {
      return false;
    }
    this.$.toolbar.searchField.showAndFocus();
    return true;
  }

  // Override FindShortcutBehavior methods.
  searchInputHasFocus(): boolean {
    return this.$.toolbar.searchField.isSearchFocused();
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(HistoryAppElement.is, HistoryAppElement);
