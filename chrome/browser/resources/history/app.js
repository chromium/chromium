// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Polymer, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {FindShortcutBehavior} from 'chrome://resources/cr_elements/find_shortcut_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import {IronScrollTargetBehavior} from 'chrome://resources/polymer/v3_0/iron-scroll-target-behavior/iron-scroll-target-behavior.js';
import {BrowserService} from './browser_service.js';
import {HistoryPageViewHistogram} from './constants.js';
import {ForeignSession, QueryResult, QueryState} from './externs.js';
import './history_list.js';
import './history_toolbar.js';
import './query_manager.js';
import './router.js';
import './shared_style.js';
import {FooterInfo} from './side_bar.js';
import './strings.js';

  let lazyLoadPromise = null;
  export function ensureLazyLoaded() {
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
      ]);
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
      document.addEventListener(eventName, function(e) {
        // Ignore buttons other than left and middle.
        if (e.button > 1 || e.defaultPrevented) {
          return;
        }

        const eventPath = e.path;
        let anchor = null;
        if (eventPath) {
          for (let i = 0; i < eventPath.length; i++) {
            const element = eventPath[i];
            if (element.tagName === 'A' && element.href) {
              anchor = element;
              break;
            }
          }
        }

        // Fallback if Event.path is not available.
        let el = e.target;
        if (!anchor && el.nodeType === Node.ELEMENT_NODE &&
            el.webkitMatchesSelector('A, A *')) {
          while (el.tagName !== 'A') {
            el = el.parentElement;
          }
          anchor = el;
        }

        if (!anchor) {
          return;
        }

        anchor = /** @type {!HTMLAnchorElement} */ (anchor);
        if ((anchor.protocol === 'file:' || anchor.protocol === 'about:') &&
            (e.button === 0 || e.button === 1)) {
          BrowserService.getInstance().navigateToUrl(
              anchor.href, anchor.target, /** @type {!MouseEvent} */ (e));
          e.preventDefault();
        }
      });
    });
  }

  Polymer({
    is: 'history-app',

    _template: html`{__html_template__}`,

    behaviors: [
      FindShortcutBehavior,
      IronScrollTargetBehavior,
      WebUIListenerBehavior,
    ],

    properties: {
      // The id of the currently selected page.
      selectedPage_: {
        type: String,
        observer: 'selectedPageChanged_',
      },

      /** @type {!QueryResult} */
      queryResult_: {
        type: Object,
        value() {
          return {
            info: null,
            results: null,
            sessionList: null,
          };
        }
      },

      isUserSignedIn_: {
        type: Boolean,
        // Updated on synced-device-manager attach by chrome.sending
        // 'otherDevicesInitialized'.
        value: loadTimeData.getBoolean('isUserSignedIn'),
      },

      /** @private */
      pendingDelete_: Boolean,

      toolbarShadow_: {
        type: Boolean,
        reflectToAttribute: true,
        notify: true,
      },

      showMenuPromo_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showMenuPromo');
        },
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
    },

    listeners: {
      'cr-toolbar-menu-promo-close': 'onCrToolbarMenuPromoClose_',
      'cr-toolbar-menu-promo-shown': 'onCrToolbarMenuPromoShown_',
      'cr-toolbar-menu-tap': 'onCrToolbarMenuTap_',
      'delete-selected': 'deleteSelected',
      'history-checkbox-select': 'checkboxSelected',
      'history-close-drawer': 'closeDrawer_',
      'history-view-changed': 'historyViewChanged_',
      'unselect-all': 'unselectAll',
    },

    /** @private {?function(!Event)} */
    boundOnKeyDown_: null,

    /** @private {?BrowserService} */
    browserService_: null,

    /** @override */
    created() {
      listenForPrivilegedLinkClicks();
    },

    /** @override */
    attached() {
      this.boundOnKeyDown_ = e => this.onKeyDown_(e);
      document.addEventListener('keydown', this.boundOnKeyDown_);
      this.addWebUIListener(
          'sign-in-state-changed',
          signedIn => this.onSignInStateChanged_(signedIn));
      this.addWebUIListener(
          'has-other-forms-changed',
          hasOtherForms => this.onHasOtherFormsChanged_(hasOtherForms));
      this.addWebUIListener(
          'foreign-sessions-changed',
          sessionList => this.setForeignSessions_(sessionList));
      this.browserService_ = BrowserService.getInstance();
      /** @type {!HistoryQueryManagerElement} */ (
          this.$$('history-query-manager'))
          .initialize();
      this.browserService_.getForeignSessions().then(
          sessionList => this.setForeignSessions_(sessionList));
    },

    /** @override */
    detached() {
      document.removeEventListener('keydown', this.boundOnKeyDown_);
      this.boundOnKeyDown_ = null;
    },

    /** @private */
    onFirstRender_() {
      setTimeout(() => {
        this.browserService_.recordTime(
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
          document.fonts.load('bold 12px Roboto');
        });
      });
    },

    /** Overridden from IronScrollTargetBehavior */
    _scrollHandler() {
      if (this.scrollTarget) {
        this.toolbarShadow_ = this.scrollTarget.scrollTop !== 0;
      }
    },

    /** @private */
    onCrToolbarMenuPromoClose_() {
      this.showMenuPromo_ = false;
    },

    /** @private */
    onCrToolbarMenuPromoShown_() {
      this.browserService_.menuPromoShown();
    },

    /** @private */
    onCrToolbarMenuTap_() {
      const drawer = /** @type {!CrDrawerElement} */ (this.$.drawer.get());
      drawer.toggle();
      this.showMenuPromo_ = false;
    },

    /**
     * Listens for history-item being selected or deselected (through checkbox)
     * and changes the view of the top toolbar.
     */
    checkboxSelected() {
      const toolbar = /** @type {HistoryToolbarElement} */ (this.$.toolbar);
      toolbar.count = /** @type {HistoryListElement} */ (this.$.history)
                          .getSelectedItemCount();
    },

    selectOrUnselectAll() {
      const list = /** @type {HistoryListElement} */ (this.$.history);
      const toolbar = /** @type {HistoryToolbarElement} */ (this.$.toolbar);
      list.selectOrUnselectAll();
      toolbar.count = list.getSelectedItemCount();
    },

    /**
     * Listens for call to cancel selection and loops through all items to set
     * checkbox to be unselected.
     * @private
     */
    unselectAll() {
      const list = /** @type {HistoryListElement} */ (this.$.history);
      const toolbar = /** @type {HistoryToolbarElement} */ (this.$.toolbar);
      list.unselectAllItems();
      toolbar.count = 0;
    },

    deleteSelected() {
      this.$.history.deleteSelectedWithPrompt();
    },

    /** @private */
    onQueryFinished_() {
      const list = /** @type {HistoryListElement} */ (this.$['history']);
      list.historyResult(
          assert(this.queryResult_.info), assert(this.queryResult_.results));
      if (document.body.classList.contains('loading')) {
        document.body.classList.remove('loading');
        this.onFirstRender_();
      }
    },

    /**
     * @param {!KeyboardEvent} e
     * @private
     */
    onKeyDown_(e) {
      if ((e.key === 'Delete' || e.key === 'Backspace') &&
          !(e.altKey || e.ctrlKey || e.metaKey || e.shiftKey)) {
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
    },

    /** @private */
    onDeleteCommand_() {
      if (this.$.toolbar.count === 0 || this.pendingDelete_) {
        return;
      }
      this.deleteSelected();
    },

    /**
     * @return {boolean} Whether the command was actually triggered.
     * @private
     */
    onSelectAllCommand_() {
      if (this.$.toolbar.searchField.isSearchFocused() ||
          this.syncedTabsSelected_(this.selectedPage_)) {
        return false;
      }
      this.selectOrUnselectAll();
      return true;
    },

    /**
     * @param {!Array<!ForeignSession>} sessionList Array of objects describing
     *     the sessions from other devices.
     * @private
     */
    setForeignSessions_(sessionList) {
      this.set('queryResult_.sessionList', sessionList);
    },

    /**
     * Update sign in state of synced device manager after user logs in or out.
     * @param {boolean} isUserSignedIn
     * @private
     */
    onSignInStateChanged_(isUserSignedIn) {
      this.isUserSignedIn_ = isUserSignedIn;
    },

    /**
     * Update sign in state of synced device manager after user logs in or out.
     * @param {boolean} hasOtherForms
     * @private
     */
    onHasOtherFormsChanged_(hasOtherForms) {
      this.set('footerInfo.otherFormsOfHistory', hasOtherForms);
    },

    /**
     * @param {string} selectedPage
     * @return {boolean}
     * @private
     */
    syncedTabsSelected_(selectedPage) {
      return selectedPage === 'syncedTabs';
    },

    /**
     * @param {boolean} querying
     * @param {boolean} incremental
     * @param {string} searchTerm
     * @return {boolean} Whether a loading spinner should be shown (implies the
     *     backend is querying a new search term).
     * @private
     */
    shouldShowSpinner_(querying, incremental, searchTerm) {
      return querying && !incremental && searchTerm !== '';
    },

    /** @private */
    selectedPageChanged_() {
      this.unselectAll();
      this.historyViewChanged_();
    },

    /** @private */
    historyViewChanged_() {
      // This allows the synced-device-manager to render so that it can be set
      // as the scroll target.
      requestAnimationFrame(() => {
        this._scrollHandler();
      });
      this.recordHistoryPageView_();
    },

    /** @private */
    hasDrawerChanged_() {
      const drawer =
          /** @type {?CrDrawerElement} */ (this.$.drawer.getIfExists());
      if (!this.hasDrawer_ && drawer && drawer.open) {
        drawer.cancel();
      }
    },

    /**
     * This computed binding is needed to make the iron-pages selector update
     * when the synced-device-manager is instantiated for the first time.
     * Otherwise the fallback selection will continue to be used after the
     * corresponding item is added as a child of iron-pages.
     * @param {string} selectedPage
     * @param {Array} items
     * @return {string}
     * @private
     */
    getSelectedPage_(selectedPage, items) {
      return selectedPage;
    },

    /** @private */
    closeDrawer_() {
      const drawer = this.$.drawer.get();
      if (drawer && drawer.open) {
        drawer.close();
      }
    },

    /** @private */
    recordHistoryPageView_() {
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

      this.browserService_.recordHistogram(
          'History.HistoryPageView', histogramValue,
          HistoryPageViewHistogram.END);
    },

    // Override FindShortcutBehavior methods.
    handleFindShortcut(modalContextOpen) {
      if (modalContextOpen) {
        return false;
      }
      this.$.toolbar.searchField.showAndFocus();
      return true;
    },

    // Override FindShortcutBehavior methods.
    searchInputHasFocus() {
      return this.$.toolbar.searchField.isSearchFocused();
    },
  });
