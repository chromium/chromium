// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
import 'chrome://resources/cr_elements/cr_splitter/cr_splitter.js';
import './folder_node.js';
import './list.js';
import '/strings.m.js';
import './command_manager.js';
import './toolbar.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrSplitterElement} from 'chrome://resources/cr_elements/cr_splitter/cr_splitter.js';
import {FindShortcutMixinLit} from 'chrome://resources/cr_elements/find_shortcut_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {setSearchResults} from './actions.js';
import {destroy as destroyApiListener, init as initApiListener} from './api_listener.js';
import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {ACCOUNT_HEADING_NODE_ID, LOCAL_STORAGE_FOLDER_STATE_KEY, LOCAL_STORAGE_TREE_WIDTH_KEY, ROOT_NODE_ID} from './constants.js';
import {DndManager} from './dnd_manager.js';
import {BookmarksRouter} from './router.js';
import {Store} from './store.js';
import {StoreClientMixinLit} from './store_client_mixin_lit.js';
import type {BookmarksPageState, FolderOpenState} from './types.js';
import {createEmptyState, normalizeNodes} from './util.js';

export const HIDE_FOCUS_RING_ATTRIBUTE = 'hide-focus-ring';

const BookmarksAppElementBase =
    StoreClientMixinLit(FindShortcutMixinLit(CrLitElement));

export interface BookmarksAppElement {
  $: {
    splitter: CrSplitterElement,
    sidebar: HTMLElement,
  };
}

export class BookmarksAppElement extends BookmarksAppElementBase {
  static get is() {
    return 'bookmarks-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      searchTerm_: {type: String},
      folderOpenState_: {type: Object},
      sidebarWidth_: {type: String},

      toolbarShadow_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  private accessor folderOpenState_: FolderOpenState|undefined;
  private accessor searchTerm_: string|undefined;
  protected accessor sidebarWidth_: string = '';
  protected accessor toolbarShadow_: boolean = false;
  private eventTracker_: EventTracker = new EventTracker();
  private dndManager_: DndManager|null = null;
  private router_: BookmarksRouter = new BookmarksRouter();

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

    // These events are added to the document because capture doesn't work
    // properly when listeners are added to a Polymer element, because the
    // event is considered AT_TARGET for the element, and is evaluated
    // after inner captures.
    this.eventTracker_.add(
        document, 'mousedown', () => this.onMousedown_(), true);
    this.eventTracker_.add(
        document, 'keydown', (e: Event) => this.onKeydown_(e as KeyboardEvent),
        true);

    this.router_.initialize();

    this.updateFromStore();

    BookmarksApiProxyImpl.getInstance().getTree().then((results) => {
      const nodeMap = normalizeNodes(results[0]!);
      const initialState = createEmptyState();
      initialState.nodes = nodeMap;

      // Select the account bookmarks bar if it exists. If not, do not set the
      // initial state so that the default is selected instead.
      const selectedFolderParent =
          nodeMap[ACCOUNT_HEADING_NODE_ID] || nodeMap[ROOT_NODE_ID];
      assert(selectedFolderParent && selectedFolderParent.children);

      for (const id of selectedFolderParent.children) {
        if (nodeMap[id]!.folderType! ===
                chrome.bookmarks.FolderType.BOOKMARKS_BAR &&
            nodeMap[id]!.syncing) {
          initialState.selectedFolder = id;
          break;
        }
      }

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
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.router_.teardown();
    this.eventTracker_.remove(document, 'mousedown');
    this.eventTracker_.remove(document, 'keydown');
    this.eventTracker_.remove(window, 'resize');
    assert(this.dndManager_);
    this.dndManager_.destroy();
    this.dndManager_ = null;
    destroyApiListener();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('searchTerm_')) {
      this.searchTermChanged_(
          this.searchTerm_,
          (changedPrivateProperties.get('searchTerm_') as string | undefined));
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('folderOpenState_')) {
      this.folderOpenStateChanged_();
    }
  }

  override onStateChanged(state: BookmarksPageState) {
    this.searchTerm_ = state.search.term;
    this.folderOpenState_ = state.folderOpenState;
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

  private onMousedown_() {
    this.toggleAttribute(HIDE_FOCUS_RING_ATTRIBUTE, true);
  }

  private onKeydown_(e: KeyboardEvent) {
    if (!['Shift', 'Alt', 'Control', 'Meta'].includes(e.key)) {
      this.toggleAttribute(HIDE_FOCUS_RING_ATTRIBUTE, false);
    }
  }

  private searchTermChanged_(newValue?: string, oldValue?: string) {
    if (oldValue !== undefined && !newValue) {
      getAnnouncerInstance().announce(loadTimeData.getString('searchCleared'));
    }

    if (!this.searchTerm_) {
      return;
    }

    const searchTerm = this.searchTerm_;
    BookmarksApiProxyImpl.getInstance().search(searchTerm).then(results => {
      const ids = results.map(node => node.id);
      this.dispatch(setSearchResults(ids));
      getAnnouncerInstance().announce(
          ids.length > 0 ?
              loadTimeData.getStringF('searchResults', searchTerm) :
              loadTimeData.getString('noSearchResults'));
    });
  }

  private folderOpenStateChanged_(): void {
    assert(this.folderOpenState_);
    window.localStorage[LOCAL_STORAGE_FOLDER_STATE_KEY] =
        JSON.stringify(Array.from(this.folderOpenState_));
  }

  // Override FindShortcutMixinLit methods.
  override handleFindShortcut(modalContextOpen: boolean): boolean {
    if (modalContextOpen) {
      return false;
    }
    this.shadowRoot.querySelector(
                       'bookmarks-toolbar')!.searchField.showAndFocus();
    return true;
  }

  // Override FindShortcutMixinLit methods.
  override searchInputHasFocus(): boolean {
    return this.shadowRoot.querySelector('bookmarks-toolbar')!.searchField
        .isSearchFocused();
  }

  protected onUndoClick_(): void {
    this.fire('command-undo');
  }

  protected onListScroll_(e: Event) {
    this.toolbarShadow_ = (e.target as HTMLElement).scrollTop !== 0;
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
