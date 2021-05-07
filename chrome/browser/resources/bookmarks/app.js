// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
import 'chrome://resources/cr_elements/cr_splitter/cr_splitter.js';
import './folder_node.js';
import './list.js';
import './router.js';
import './shared_vars.js';
import './strings.m.js';
import './command_manager.js';

import {FindShortcutBehavior, FindShortcutBehaviorInterface} from 'chrome://resources/cr_elements/find_shortcut_behavior.js';
import {StoreObserver} from 'chrome://resources/js/cr/ui/store.m.js';
import {StoreClientInterface as CrUiStoreClientInterface} from 'chrome://resources/js/cr/ui/store_client.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {setSearchResults} from './actions.js';
import {destroy as destroyApiListener, init as initApiListener} from './api_listener.js';
import {LOCAL_STORAGE_FOLDER_STATE_KEY, LOCAL_STORAGE_TREE_WIDTH_KEY, ROOT_NODE_ID} from './constants.js';
import {DNDManager} from './dnd_manager.js';
import {MouseFocusBehavior} from './mouse_focus_behavior.js';
import {Store} from './store.js';
import {BookmarksStoreClientInterface, StoreClient} from './store_client.js';
import {BookmarksToolbarElement} from './toolbar.js';
import {BookmarksPageState, FolderOpenState} from './types.js';
import {createEmptyState, normalizeNodes} from './util.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {BookmarksStoreClientInterface}
 * @implements {CrUiStoreClientInterface}
 * @implements {StoreObserver<BookmarksPageState>}
 * @implements {FindShortcutBehaviorInterface}
 */
const BookmarksAppElementBase = mixinBehaviors(
    [StoreClient, MouseFocusBehavior, FindShortcutBehavior], PolymerElement);

/** @polymer */
export class BookmarksAppElement extends BookmarksAppElementBase {
  static get is() {
    return 'bookmarks-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      searchTerm_: {
        type: String,
        observer: 'searchTermChanged_',
      },

      /** @type {FolderOpenState} */
      folderOpenState_: {
        type: Object,
        observer: 'folderOpenStateChanged_',
      },

      /** @private */
      sidebarWidth_: String,
    };
  }

  constructor() {
    super();

    /** @private{?function(!Event)} */
    this.boundUpdateSidebarWidth_ = null;

    /** @private {DNDManager} */
    this.dndManager_ = null;

    // Regular expression that captures the leading slash, the content and the
    // trailing slash in three different groups.
    const CANONICAL_PATH_REGEX = /(^\/)([\/-\w]+)(\/$)/;
    const path = location.pathname.replace(CANONICAL_PATH_REGEX, '$1$2');
    if (path !== '/') {  // Only queries are supported, not subpages.
      window.history.replaceState(undefined /* stateObject */, '', '/');
    }
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    document.documentElement.classList.remove('loading');

    this.watch('searchTerm_', function(state) {
      return state.search.term;
    });

    this.watch('folderOpenState_', function(state) {
      return state.folderOpenState;
    });

    chrome.bookmarks.getTree((results) => {
      const nodeMap = normalizeNodes(results[0]);
      const initialState = createEmptyState();
      initialState.nodes = nodeMap;
      initialState.selectedFolder = nodeMap[ROOT_NODE_ID].children[0];
      const folderStateString =
          window.localStorage[LOCAL_STORAGE_FOLDER_STATE_KEY];
      initialState.folderOpenState = folderStateString ?
          new Map(
              /** @type Array<Array<boolean|string>> */ (
                  JSON.parse(folderStateString))) :
          new Map();

      Store.getInstance().init(initialState);
      initApiListener();

      setTimeout(function() {
        chrome.metricsPrivate.recordTime(
            'BookmarkManager.ResultsRenderedTime',
            Math.floor(window.performance.now()));
      });
    });

    this.boundUpdateSidebarWidth_ = this.updateSidebarWidth_.bind(this);

    this.initializeSplitter_();

    this.dndManager_ = new DNDManager();
    this.dndManager_.init();
  }

  disconnectedCallback() {
    super.disconnectedCallback();

    window.removeEventListener('resize', this.boundUpdateSidebarWidth_);
    this.dndManager_.destroy();
    destroyApiListener();
  }

  /**
   * Set up the splitter and set the initial width from localStorage.
   * @private
   */
  initializeSplitter_() {
    const splitter = this.$.splitter;
    const splitterTarget = this.$.sidebar;

    // The splitter persists the size of the left component in the local store.
    if (LOCAL_STORAGE_TREE_WIDTH_KEY in window.localStorage) {
      splitterTarget.style.width =
          window.localStorage[LOCAL_STORAGE_TREE_WIDTH_KEY];
    }
    this.sidebarWidth_ =
        /** @type {string} */ (getComputedStyle(splitterTarget).width);

    splitter.addEventListener('resize', (e) => {
      window.localStorage[LOCAL_STORAGE_TREE_WIDTH_KEY] =
          splitterTarget.style.width;
      this.updateSidebarWidth_();
    });

    splitter.addEventListener('dragmove', this.boundUpdateSidebarWidth_);
    window.addEventListener('resize', this.boundUpdateSidebarWidth_);
  }

  /** @private */
  updateSidebarWidth_() {
    this.sidebarWidth_ =
        /** @type {string} */ (getComputedStyle(this.$.sidebar).width);
  }

  /** @private */
  searchTermChanged_(newValue, oldValue) {
    if (oldValue !== undefined && !newValue) {
      this.dispatchEvent(new CustomEvent('iron-announce', {
        bubbles: true,
        composed: true,
        detail: {text: loadTimeData.getString('searchCleared')}
      }));
    }

    if (!this.searchTerm_) {
      return;
    }

    chrome.bookmarks.search(this.searchTerm_, (results) => {
      const ids = results.map(function(node) {
        return node.id;
      });
      this.dispatch(setSearchResults(ids));
      this.dispatchEvent(new CustomEvent('iron-announce', {
        bubbles: true,
        composed: true,
        detail: {
          text: ids.length > 0 ?
              loadTimeData.getStringF('searchResults', this.searchTerm_) :
              loadTimeData.getString('noSearchResults')
        }
      }));
    });
  }

  /** @private */
  folderOpenStateChanged_() {
    window.localStorage[LOCAL_STORAGE_FOLDER_STATE_KEY] =
        JSON.stringify(Array.from(this.folderOpenState_));
  }

  // Override FindShortcutBehavior methods.
  /** @override */
  handleFindShortcut(modalContextOpen) {
    if (modalContextOpen) {
      return false;
    }
    /** @type {!BookmarksToolbarElement} */ (
        this.shadowRoot.querySelector('bookmarks-toolbar'))
        .searchField.showAndFocus();
    return true;
  }

  // Override FindShortcutBehavior methods.
  /** @override */
  searchInputHasFocus() {
    return /** @type {!BookmarksToolbarElement} */ (
               this.shadowRoot.querySelector('bookmarks-toolbar'))
        .searchField.isSearchFocused();
  }

  /** @private */
  onUndoClick_() {
    this.dispatchEvent(
        new CustomEvent('command-undo', {bubbles: true, composed: true}));
  }
}

customElements.define(BookmarksAppElement.is, BookmarksAppElement);
