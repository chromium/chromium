// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
import 'chrome://resources/cr_elements/cr_splitter/cr_splitter.js';
import './folder_node.js';
import './list.js';
import './router.js';
import './shared_vars.css.js';
import './strings.m.js';
import './command_manager.js';
import './toolbar.js';

import type {CrSplitterElement} from 'chrome://resources/cr_elements/cr_splitter/cr_splitter.js';
import type {FindShortcutListener} from 'chrome://resources/cr_elements/find_shortcut_manager.js';
import {FindShortcutMixin} from 'chrome://resources/cr_elements/find_shortcut_mixin.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {IronScrollTargetBehavior} from 'chrome://resources/polymer/v3_0/iron-scroll-target-behavior/iron-scroll-target-behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {setSearchResults} from './actions.js';
import {destroy as destroyApiListener, init as initApiListener} from './api_listener.js';
import {getTemplate} from './app.html.js';
import {BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {LOCAL_STORAGE_FOLDER_STATE_KEY, LOCAL_STORAGE_TREE_WIDTH_KEY, ROOT_NODE_ID} from './constants.js';
import {DndManager} from './dnd_manager.js';
import type {MouseFocusMixinInterface} from './mouse_focus_behavior.js';
import {MouseFocusMixin} from './mouse_focus_behavior.js';
import {Store} from './store.js';
import type {StoreClientMixinInterface} from './store_client_mixin.js';
import {StoreClientMixin} from './store_client_mixin.js';
import type {BookmarksToolbarElement} from './toolbar.js';
import type {BookmarksPageState, FolderOpenState} from './types.js';
import {createEmptyState, normalizeNodes} from './util.js';

const BookmarksAppElementBase =
    mixinBehaviors(
        [IronScrollTargetBehavior],
        StoreClientMixin(MouseFocusMixin(FindShortcutMixin(PolymerElement)))) as
    {
      new (): PolymerElement & StoreClientMixinInterface &
          FindShortcutListener & IronScrollTargetBehavior &
          MouseFocusMixinInterface,
    };

export interface BookmarksAppElement {
  $: {
    splitter: CrSplitterElement,
    sidebar: HTMLDivElement,
  };
}

export class BookmarksAppElement extends BookmarksAppElementBase {
  static get is() {
    return 'bookmarks-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      searchTerm_: {
        type: String,
        observer: 'searchTermChanged_',
      },

      folderOpenState_: {
        type: Object,
        observer: 'folderOpenStateChanged_',
      },

      sidebarWidth_: String,

      toolbarShadow_: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  private eventTracker_: EventTracker = new EventTracker();
  private dndManager_: DndManager|null = null;
  private folderOpenState_: FolderOpenState;
  private searchTerm_: string;
  private sidebarWidth_: string;
  private toolbarShadow_: boolean;

  constructor() {
    super();

    // Regular expression that captures the leading slash, the content and the
    // trailing slash in three different groups.
    const CANONICAL_PATH_REGEX = /(^\/)([\/-\w]+)(\/$)/;
    const path = location.pathname.replace(CANONICAL_PATH_REGEX, '$1$2');
    if (path !== '/') {  // Only queries are supported, not subpages.
      window.history.replaceState(undefined /* stateObject */, '', '/');
    }
  }

  override connectedCallback() {
    super.connectedCallback();

    document.documentElement.classList.remove('loading');

    this.watch('searchTerm_', function(state: BookmarksPageState) {
      return state.search.term;
    });

    this.watch('folderOpenState_', function(state: BookmarksPageState) {
      return state.folderOpenState;
    });

    BookmarksApiProxyImpl.getInstance().getTree().then((results) => {
      const nodeMap = normalizeNodes(results[0]!);
      const initialState = createEmptyState();
      initialState.nodes = nodeMap;
      initialState.selectedFolder = nodeMap[ROOT_NODE_ID]!.children![0]!;
      const folderStateString =
          window.localStorage[LOCAL_STORAGE_FOLDER_STATE_KEY];
      initialState.folderOpenState = folderStateString ?
          new Map(JSON.parse(folderStateString)) :
          new Map();

      Store.getInstance().init(initialState);
      initApiListener();

      setTimeout(function() {
        chrome.metricsPrivate.recordTime(
            'BookmarkManager.ResultsRenderedTime',
            Math.floor(window.performance.now()));
      });
    });

    this.initializeSplitter_();

    this.dndManager_ = new DndManager();
    this.dndManager_.init();

    this.scrollTarget = this.shadowRoot!.querySelector('bookmarks-list');
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.eventTracker_.remove(window, 'resize');
    this.dndManager_!.destroy();
    destroyApiListener();
  }

  private initializeSplitter_(): void {
    const splitter = this.$.splitter;
    const splitterTarget = this.$.sidebar;

    // The splitter persists the size of the left component in the local store.
    if (LOCAL_STORAGE_TREE_WIDTH_KEY in window.localStorage) {
      splitterTarget.style.width =
          window.localStorage[LOCAL_STORAGE_TREE_WIDTH_KEY];
    }
    this.sidebarWidth_ = getComputedStyle(splitterTarget).width;

    splitter.addEventListener('resize', (_e: Event) => {
      window.localStorage[LOCAL_STORAGE_TREE_WIDTH_KEY] =
          splitterTarget.style.width;
      this.updateSidebarWidth_();
    });

    this.eventTracker_.add(splitter, 'dragmove',
                           () => this.updateSidebarWidth_());
    this.eventTracker_.add(window, 'resize', () => this.updateSidebarWidth_());
  }

  private updateSidebarWidth_(): void {
    this.sidebarWidth_ = getComputedStyle(this.$.sidebar).width;
  }

  private searchTermChanged_(newValue: string, oldValue?: string) {
    if (oldValue !== undefined && !newValue) {
      this.dispatchEvent(new CustomEvent('iron-announce', {
        bubbles: true,
        composed: true,
        detail: {text: loadTimeData.getString('searchCleared')},
      }));
    }

    if (!this.searchTerm_) {
      return;
    }

    BookmarksApiProxyImpl.getInstance()
        .search(this.searchTerm_)
        .then(results => {
          const ids = results.map(node => node.id);
          this.dispatch(setSearchResults(ids));
          this.dispatchEvent(new CustomEvent('iron-announce', {
            bubbles: true,
            composed: true,
            detail: {
              text: ids.length > 0 ?
                  loadTimeData.getStringF('searchResults', this.searchTerm_) :
                  loadTimeData.getString('noSearchResults'),
            },
          }));
        });
  }

  private folderOpenStateChanged_(): void {
    window.localStorage[LOCAL_STORAGE_FOLDER_STATE_KEY] =
        JSON.stringify(Array.from(this.folderOpenState_));
  }

  // Override FindShortcutMixin methods.
  override handleFindShortcut(modalContextOpen: boolean): boolean {
    if (modalContextOpen) {
      return false;
    }
    this.shadowRoot!.querySelector<BookmarksToolbarElement>(
        'bookmarks-toolbar')!.searchField.showAndFocus();
    return true;
  }

  // Override FindShortcutMixin methods.
  override searchInputHasFocus(): boolean {
    return this.shadowRoot!.querySelector<BookmarksToolbarElement>(
        'bookmarks-toolbar')!.searchField.isSearchFocused();
  }

  private onUndoClick_(): void {
    this.dispatchEvent(
        new CustomEvent('command-undo', {bubbles: true, composed: true}));
  }

  /** Overridden from IronScrollTargetBehavior */
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _scrollHandler() {
    this.toolbarShadow_ = this.scrollTarget!.scrollTop !== 0;
  }

  getDndManagerForTesting(): DndManager|null {
    return this.dndManager_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'bookmarks-app': BookmarksAppElement;
  }
}

customElements.define(BookmarksAppElement.is, BookmarksAppElement);
