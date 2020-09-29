// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/managed_footnote/managed_footnote.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
import 'chrome://resources/cr_elements/cr_splitter/cr_splitter.js';
import './folder_node.js';
import './list.js';
import './router.js';
import './shared_vars.js';
import './strings.m.js';
import './toolbar.js';

import {FindShortcutBehavior} from 'chrome://resources/cr_elements/find_shortcut_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {setSearchResults} from './actions.js';
import {destroy as destroyApiListener, init as initApiListener} from './api_listener.js';
import {CommandManager} from './command_manager.js';
import {LOCAL_STORAGE_FOLDER_STATE_KEY, LOCAL_STORAGE_TREE_WIDTH_KEY, ROOT_NODE_ID} from './constants.js';
import {DNDManager} from './dnd_manager.js';
import {MouseFocusBehavior} from './mouse_focus_behavior.js';
import {Store} from './store.js';
import {StoreClient} from './store_client.js';
import {FolderOpenState} from './types.js';
import {createEmptyState, normalizeNodes} from './util.js';

Polymer({
  is: 'bookmarks-app',

  _template: html`{__html_template__}`,

  behaviors: [
    MouseFocusBehavior,
    StoreClient,
    FindShortcutBehavior,
  ],

  properties: {
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
  },

  /** @private{?function(!Event)} */
  boundUpdateSidebarWidth_: null,

  /** @private {DNDManager} */
  dndManager_: null,

  /** @override */
  created() {
    // Regular expression that captures the leading slash, the content and the
    // trailing slash in three different groups.
    const CANONICAL_PATH_REGEX = /(^\/)([\/-\w]+)(\/$)/;
    const path = location.pathname.replace(CANONICAL_PATH_REGEX, '$1$2');
    if (path !== '/') {  // Only queries are supported, not subpages.
      window.history.replaceState(undefined /* stateObject */, '', '/');
    }
  },

  /** @override */
  attached() {
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
  },

  detached() {
    window.removeEventListener('resize', this.boundUpdateSidebarWidth_);
    this.dndManager_.destroy();
    destroyApiListener();
  },

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
  },

  /** @private */
  updateSidebarWidth_() {
    this.sidebarWidth_ =
        /** @type {string} */ (getComputedStyle(this.$.sidebar).width);
  },

  /** @private */
  searchTermChanged_(newValue, oldValue) {
    if (oldValue !== undefined && !newValue) {
      this.fire(
          'iron-announce', {text: loadTimeData.getString('searchCleared')});
    }

    if (!this.searchTerm_) {
      return;
    }

    chrome.bookmarks.search(this.searchTerm_, (results) => {
      const ids = results.map(function(node) {
        return node.id;
      });
      this.dispatch(setSearchResults(ids));
      this.fire('iron-announce', {
        text: ids.length > 0 ?
            loadTimeData.getStringF('searchResults', this.searchTerm_) :
            loadTimeData.getString('noSearchResults')
      });
    });
  },

  /** @private */
  folderOpenStateChanged_() {
    window.localStorage[LOCAL_STORAGE_FOLDER_STATE_KEY] =
        JSON.stringify(Array.from(this.folderOpenState_));
  },

  // Override FindShortcutBehavior methods.
  handleFindShortcut(modalContextOpen) {
    if (modalContextOpen) {
      return false;
    }
    this.$$('bookmarks-toolbar').searchField.showAndFocus();
    return true;
  },

  // Override FindShortcutBehavior methods.
  searchInputHasFocus() {
    return this.$$('bookmarks-toolbar').searchField.isSearchFocused();
  },

  /** @private */
  onUndoClick_() {
    this.fire('command-undo');
  },
});
