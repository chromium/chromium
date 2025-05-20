// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Element which shows context menus and handles keyboard
 * shortcuts.
 */
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '/strings.m.js';
import './edit_dialog.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {KeyboardShortcutList} from 'chrome://resources/js/keyboard_shortcut_list.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {deselectItems, selectAll, selectFolder} from './actions.js';
import {highlightUpdatedItems, trackUpdatedItems} from './api_listener.js';
import {BookmarkManagerApiProxyImpl} from './bookmark_manager_api_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {getHtml} from './command_manager.html.js';
import {Command, IncognitoAvailability, MenuSource, OPEN_CONFIRMATION_LIMIT} from './constants.js';
import {DialogFocusManager} from './dialog_focus_manager.js';
import type {BookmarksEditDialogElement} from './edit_dialog.js';
import {getCss as getSharedStyleCss} from './shared_style_lit.css.js';
import {StoreClientMixinLit} from './store_client_mixin_lit.js';
import type {BookmarkNode, BookmarksPageState, OpenCommandMenuDetail} from './types.js';
import {canEditNode, canReorderChildren, getDisplayedList, isRootNode, isRootOrChildOfRoot} from './util.js';

const BookmarksCommandManagerElementBase = StoreClientMixinLit(CrLitElement);

export interface BookmarksCommandManagerElement {
  $: {
    dropdown: CrLazyRenderLitElement<CrActionMenuElement>,
  };
}

let instance: BookmarksCommandManagerElement|null = null;

export class BookmarksCommandManagerElement extends
    BookmarksCommandManagerElementBase {
  static get is() {
    return 'bookmarks-command-manager';
  }

  static override get styles() {
    return getSharedStyleCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      menuIds_: {type: Object},
      menuSource_: {type: Number},
      canPaste_: {type: Boolean},
      isActiveTabInSplit_: {type: Boolean},
      globalCanEdit_: {type: Boolean},
      showEditDialog_: {type: Boolean},
      showOpenDialog_: {type: Boolean},
    };
  }

  /**
   * Indicates where the context menu was opened from. Will be NONE if
   * menu is not open, indicating that commands are from keyboard shortcuts
   * or elsewhere in the UI.
   */
  private accessor menuSource_: MenuSource = MenuSource.NONE;
  private confirmOpenCallback_: (() => void)|null = null;
  private accessor canPaste_: boolean = false;
  private accessor isActiveTabInSplit_: boolean = false;
  private accessor globalCanEdit_: boolean = false;
  protected accessor menuIds_: Set<string> = new Set<string>();
  protected accessor showEditDialog_: boolean = false;
  protected accessor showOpenDialog_: boolean = false;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private shortcuts_: Map<Command, KeyboardShortcutList> = new Map();
  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();
    assert(instance === null);
    instance = this;

    this.updateFromStore();

    this.addShortcut_(Command.EDIT, 'F2', 'Enter');
    this.addShortcut_(Command.DELETE, 'Delete', 'Delete Backspace');

    this.addShortcut_(Command.OPEN, 'Enter', 'Meta|o');
    this.addShortcut_(Command.OPEN_NEW_TAB, 'Ctrl|Enter', 'Meta|Enter');
    this.addShortcut_(Command.OPEN_NEW_WINDOW, 'Shift|Enter');

    // Note: the undo shortcut is also defined in bookmarks_ui.cc
    // TODO(b/893033): de-duplicate shortcut by moving all shortcut
    // definitions from JS to C++.
    this.addShortcut_(Command.UNDO, 'Ctrl|z', 'Meta|z');
    this.addShortcut_(Command.REDO, 'Ctrl|y Ctrl|Shift|Z', 'Meta|Shift|Z');

    this.addShortcut_(Command.SELECT_ALL, 'Ctrl|a', 'Meta|a');
    this.addShortcut_(Command.DESELECT_ALL, 'Escape');

    this.addShortcut_(Command.CUT, 'Ctrl|x', 'Meta|x');
    this.addShortcut_(Command.COPY, 'Ctrl|c', 'Meta|c');
    this.addShortcut_(Command.PASTE, 'Ctrl|v', 'Meta|v');

    this.eventTracker_.add(document, 'open-command-menu', (e: Event) =>
                           this.onOpenCommandMenu_(
                               e as CustomEvent<OpenCommandMenuDetail>));
    this.eventTracker_.add(document, 'keydown', (e: Event) =>
                           this.onKeydown_(e as KeyboardEvent));

    const addDocumentListenerForCommand = (eventName: string,
                                           command: Command) => {
      this.eventTracker_.add(document, eventName, (e: Event) => {
        if ((e.composedPath()[0] as HTMLElement).tagName === 'INPUT') {
          return;
        }

        const items = this.getState().selection.items;
        if (this.canExecute(command, items)) {
          this.handle(command, items);
        }
      });
    };
    addDocumentListenerForCommand('command-undo', Command.UNDO);
    addDocumentListenerForCommand('cut', Command.CUT);
    addDocumentListenerForCommand('copy', Command.COPY);
    addDocumentListenerForCommand('paste', Command.PASTE);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    instance = null;
    this.eventTracker_.removeAll();
    this.menuIds_.clear();
    this.shortcuts_.clear();
  }

  override onStateChanged(state: BookmarksPageState) {
    this.globalCanEdit_ = state.prefs.canEdit;
  }

  getMenuIdsForTesting(): Set<string> {
    return this.menuIds_;
  }

  getMenuSourceForTesting(): MenuSource {
    return this.menuSource_;
  }

  /**
   * Display the command context menu at (|x|, |y|) in window coordinates.
   * Commands will execute on |items| if given, or on the currently selected
   * items.
   */
  openCommandMenuAtPosition(
      x: number, y: number, source: MenuSource, items?: Set<string>) {
    this.menuSource_ = source;
    this.menuIds_ = items || this.getState().selection.items;

    // Wait for the changes above to reflect in the DOM before showing the menu.
    this.updateComplete.then(() => {
      const dropdown = this.$.dropdown.get();
      DialogFocusManager.getInstance().showDialog(
          dropdown.getDialog(), function() {
            dropdown.showAtPosition({top: y, left: x});
          });
    });
  }

  /**
   * Display the command context menu positioned to cover the |target|
   * element. Commands will execute on the currently selected items.
   */
  openCommandMenuAtElement(target: HTMLElement, source: MenuSource) {
    this.menuSource_ = source;
    this.menuIds_ = this.getState().selection.items;

    // Wait for the changes above to reflect in the DOM before showing the menu.
    this.updateComplete.then(() => {
      const dropdown = this.$.dropdown.get();
      DialogFocusManager.getInstance().showDialog(
          dropdown.getDialog(), function() {
            dropdown.showAt(target);
          });
    });
  }

  closeCommandMenu() {
    this.menuIds_ = new Set();
    this.menuSource_ = MenuSource.NONE;
    this.$.dropdown.get().close();
  }

  ////////////////////////////////////////////////////////////////////////////
  // Command handlers:

  /**
   * Determine if the |command| can be executed with the given |itemIds|.
   * Commands which appear in the context menu should be implemented
   * separately using `isCommandVisible_` and `isCommandEnabled_`.
   */
  canExecute(command: Command, itemIds: Set<string>): boolean {
    const state = this.getState();
    switch (command) {
      case Command.OPEN:
        return itemIds.size > 0;
      case Command.UNDO:
      case Command.REDO:
        return this.globalCanEdit_;
      case Command.SELECT_ALL:
      case Command.DESELECT_ALL:
        return true;
      case Command.COPY:
      case Command.CUT:
        return itemIds.size > 0 && this.isCommandEnabled_(command, itemIds);
      case Command.PASTE:
        return state.search.term === '' &&
            canReorderChildren(state, state.selectedFolder);
      default:
        return this.isCommandVisible_(command, itemIds) &&
            this.isCommandEnabled_(command, itemIds);
    }
  }

  protected isCommandVisible_(command: Command, itemIds: Set<string>): boolean {
    switch (command) {
      case Command.EDIT:
        return itemIds.size === 1 && this.globalCanEdit_;
      case Command.PASTE:
        return this.globalCanEdit_;
      case Command.CUT:
      case Command.COPY:
        return itemIds.size >= 1 && this.globalCanEdit_;
      case Command.DELETE:
        return itemIds.size > 0 && this.globalCanEdit_;
      case Command.SHOW_IN_FOLDER:
        return this.menuSource_ === MenuSource.ITEM && itemIds.size === 1 &&
            this.getState().search.term !== '' &&
            !isRootOrChildOfRoot(this.getState(), Array.from(itemIds)[0]!);
      case Command.OPEN_INCOGNITO:
      case Command.OPEN_NEW_GROUP:
      case Command.OPEN_NEW_TAB:
      case Command.OPEN_NEW_WINDOW:
      case Command.OPEN_SPLIT_VIEW:
        return itemIds.size > 0;
      case Command.ADD_BOOKMARK:
      case Command.ADD_FOLDER:
      case Command.SORT:
      case Command.EXPORT:
      case Command.IMPORT:
      case Command.HELP_CENTER:
        return true;
    }
    assertNotReached();
  }

  protected isCommandEnabled_(command: Command, itemIds: Set<string>): boolean {
    const state = this.getState();
    switch (command) {
      case Command.EDIT:
      case Command.DELETE:
      case Command.CUT:
        return !this.containsMatchingNode_(itemIds, function(node) {
          return !canEditNode(state, node.id);
        });
      case Command.OPEN_NEW_GROUP:
      case Command.OPEN_NEW_TAB:
      case Command.OPEN_NEW_WINDOW:
        return this.expandIds_(itemIds).length > 0;
      case Command.OPEN_INCOGNITO:
        return this.expandIds_(itemIds).length > 0 &&
            state.prefs.incognitoAvailability !==
            IncognitoAvailability.DISABLED;
      case Command.OPEN_SPLIT_VIEW:
        return this.expandIds_(itemIds).length === 1 &&
            !this.isActiveTabInSplit_;
      case Command.SORT:
        return this.canChangeList_() &&
            state.nodes[state.selectedFolder]!.children!.length > 1;
      case Command.ADD_BOOKMARK:
      case Command.ADD_FOLDER:
        return this.canChangeList_();
      case Command.IMPORT:
        return this.globalCanEdit_;
      case Command.COPY:
        return !this.containsMatchingNode_(itemIds, function(node) {
          return isRootNode(node.id);
        });
      case Command.PASTE:
        return this.canPaste_;
      default:
        return true;
    }
  }

  /**
   * Returns whether the currently displayed bookmarks list can be changed.
   */
  private canChangeList_(): boolean {
    const state = this.getState();
    return state.search.term === '' &&
        canReorderChildren(state, state.selectedFolder);
  }

  private async ensureEditDialog_(): Promise<BookmarksEditDialogElement> {
    if (!this.showEditDialog_) {
      this.showEditDialog_ = true;
      await this.updateComplete;
    }
    const editDialog = this.shadowRoot.querySelector('bookmarks-edit-dialog');
    assert(editDialog);
    return editDialog;
  }

  private async ensureOpenDialog_(): Promise<CrDialogElement> {
    if (!this.showOpenDialog_) {
      this.showOpenDialog_ = true;
      await this.updateComplete;
    }
    const openDialog = this.shadowRoot.querySelector('cr-dialog');
    assert(openDialog);
    return openDialog;
  }

  handle(command: Command, itemIds: Set<string>) {
    const state = this.getState();
    switch (command) {
      case Command.EDIT: {
        const id = Array.from(itemIds)[0]!;
        this.ensureEditDialog_().then(
            dialog => dialog.showEditDialog(state.nodes[id]!));
        break;
      }
      case Command.COPY: {
        const idList = Array.from(itemIds);
        BookmarkManagerApiProxyImpl.getInstance().copy(idList).then(() => {
          let labelPromise: Promise<string>;
          if (idList.length === 1) {
            labelPromise =
                Promise.resolve(loadTimeData.getString('toastItemCopied'));
          } else {
            labelPromise = PluralStringProxyImpl.getInstance().getPluralString(
                'toastItemsCopied', idList.length);
          }

          this.showTitleToast_(
              labelPromise, state.nodes[idList[0]!]!.title, false);
        });
        break;
      }
      case Command.SHOW_IN_FOLDER: {
        const id = Array.from(itemIds)[0]!;
        const parentId = state.nodes[id]!.parentId;
        assert(parentId);
        this.dispatch(selectFolder(parentId, state.nodes));
        DialogFocusManager.getInstance().clearFocus();
        this.dispatchEvent(new CustomEvent(
            'highlight-items', {bubbles: true, composed: true, detail: [id]}));
        break;
      }
      case Command.DELETE: {
        const idList = Array.from(this.minimizeDeletionSet_(itemIds));
        const title = state.nodes[idList[0]!]!.title;
        let labelPromise: Promise<string>;

        if (idList.length === 1) {
          labelPromise =
              Promise.resolve(loadTimeData.getString('toastItemDeleted'));
        } else {
          labelPromise = PluralStringProxyImpl.getInstance().getPluralString(
              'toastItemsDeleted', idList.length);
        }

        BookmarkManagerApiProxyImpl.getInstance().removeTrees(idList).then(
            () => {
              this.showTitleToast_(labelPromise, title, true);
            });
        break;
      }
      case Command.UNDO:
        chrome.bookmarkManagerPrivate.undo();
        getToastManager().hide();
        break;
      case Command.REDO:
        chrome.bookmarkManagerPrivate.redo();
        break;
      case Command.OPEN_INCOGNITO:
      case Command.OPEN_NEW_TAB:
      case Command.OPEN_NEW_WINDOW:
      case Command.OPEN_SPLIT_VIEW:
        this.openBookmarkIds_(this.expandIds_(itemIds), command);
        break;
      case Command.OPEN_NEW_GROUP:
        // Do not expand itemsIds because the folder node is needed to associate
        // with a tab group.
        this.openBookmarkIds_(Array.from(itemIds), command);
        break;
      case Command.OPEN:
        if (this.isFolder_(itemIds)) {
          const folderId = Array.from(itemIds)[0]!;
          this.dispatch(selectFolder(folderId, state.nodes));
        } else {
          this.openBookmarkIds_(Array.from(itemIds), command);
        }
        break;
      case Command.SELECT_ALL:
        const displayedIds = getDisplayedList(state);
        this.dispatch(selectAll(displayedIds, state));
        break;
      case Command.DESELECT_ALL:
        this.dispatch(deselectItems());
        getAnnouncerInstance().announce(
            loadTimeData.getString('itemsUnselected'));
        break;
      case Command.CUT:
        BookmarkManagerApiProxyImpl.getInstance().cut(Array.from(itemIds));
        break;
      case Command.PASTE:
        const selectedFolder = state.selectedFolder;
        const selectedItems = state.selection.items;
        trackUpdatedItems();
        BookmarkManagerApiProxyImpl.getInstance()
            .paste(selectedFolder, Array.from(selectedItems))
            .then(highlightUpdatedItems);
        break;
      case Command.SORT:
        chrome.bookmarkManagerPrivate.sortChildren(state.selectedFolder);
        getToastManager().show(loadTimeData.getString('toastFolderSorted'));
        break;
      case Command.ADD_BOOKMARK:
        this.ensureEditDialog_().then(
            dialog => dialog.showAddDialog(false, state.selectedFolder));
        break;
      case Command.ADD_FOLDER:
        this.ensureEditDialog_().then(
            dialog => dialog.showAddDialog(true, state.selectedFolder));
        break;
      case Command.IMPORT:
        chrome.bookmarkManagerPrivate.import();
        break;
      case Command.EXPORT:
        chrome.bookmarkManagerPrivate.export();
        break;
      case Command.HELP_CENTER:
        window.open('https://support.google.com/chrome/?p=bookmarks');
        break;
      default:
        assertNotReached();
    }
    this.recordCommandHistogram_(
        itemIds, 'BookmarkManager.CommandExecuted', command);
  }

  handleKeyEvent(e: KeyboardEvent, itemIds: Set<string>): boolean {
    for (const commandTuple of this.shortcuts_) {
      const command = commandTuple[0];
      const shortcut = commandTuple[1];
      if (shortcut.matchesEvent(e) && this.canExecute(command, itemIds)) {
        this.handle(command, itemIds);

        e.stopPropagation();
        e.preventDefault();
        return true;
      }
    }

    return false;
  }

  ////////////////////////////////////////////////////////////////////////////
  // Private functions:

  /**
   * Register a keyboard shortcut for a command.
   */
  private addShortcut_(
      command: Command, shortcut: string, macShortcut?: string) {
    shortcut = (isMac && macShortcut) ? macShortcut : shortcut;
    this.shortcuts_.set(command, new KeyboardShortcutList(shortcut));
  }

  /**
   * Minimize the set of |itemIds| by removing any node which has an ancestor
   * node already in the set. This ensures that instead of trying to delete
   * both a node and its descendant, we will only try to delete the topmost
   * node, preventing an error in the bookmarkManagerPrivate.removeTrees API
   * call.
   */
  private minimizeDeletionSet_(itemIds: Set<string>): Set<string> {
    const minimizedSet = new Set<string>();
    const nodes = this.getState().nodes;
    itemIds.forEach(function(itemId) {
      let currentId = itemId;
      while (!isRootNode(currentId)) {
        const parentId = nodes[currentId]!.parentId;
        assert(parentId);
        currentId = parentId;
        if (itemIds.has(currentId)) {
          return;
        }
      }
      minimizedSet.add(itemId);
    });
    return minimizedSet;
  }

  /**
   * Open the given |ids| in response to a |command|. May show a confirmation
   * dialog before opening large numbers of URLs.
   */
  private openBookmarkIds_(ids: string[], command: Command) {
    assert(
        command === Command.OPEN || command === Command.OPEN_NEW_TAB ||
        command === Command.OPEN_NEW_WINDOW ||
        command === Command.OPEN_INCOGNITO ||
        command === Command.OPEN_SPLIT_VIEW ||
        command === Command.OPEN_NEW_GROUP);

    if (ids.length === 0) {
      return;
    }

    if (command === Command.OPEN_SPLIT_VIEW) {
      assert(ids.length === 1);
    }

    const openBookmarkIdsCallback = function() {
      const incognito = command === Command.OPEN_INCOGNITO;
      if (command === Command.OPEN_NEW_WINDOW || incognito) {
        BookmarkManagerApiProxyImpl.getInstance().openInNewWindow(
            ids, incognito);
      } else if (command === Command.OPEN_SPLIT_VIEW) {
        BookmarkManagerApiProxyImpl.getInstance().openInNewTab(
            ids.shift()!, {active: false, split: true});
      } else if (command === Command.OPEN_NEW_GROUP) {
        BookmarkManagerApiProxyImpl.getInstance().openInNewTabGroup(ids);
      } else {
        if (command === Command.OPEN) {
          BookmarkManagerApiProxyImpl.getInstance().openInNewTab(
              ids.shift()!, {active: true, split: false});
        }
        ids.forEach(function(id) {
          BookmarkManagerApiProxyImpl.getInstance().openInNewTab(
              id, {active: false, split: false});
        });
      }
    };

    if (ids.length <= OPEN_CONFIRMATION_LIMIT) {
      openBookmarkIdsCallback();
      return;
    }

    this.confirmOpenCallback_ = openBookmarkIdsCallback;
    this.ensureOpenDialog_().then(dialog => {
      dialog.querySelector('[slot=body]')!.textContent =
          loadTimeData.getStringF('openDialogBody', ids.length);
      DialogFocusManager.getInstance().showDialog(dialog);
    });
  }

  /**
   * Returns all ids in the given set of nodes and their immediate children.
   * Note that these will be ordered by insertion order into the |itemIds|
   * set, and that it is possible to duplicate a id by passing in both the
   * parent ID and child ID.
   */
  private expandIds_(itemIds: Set<string>): string[] {
    const result: string[] = [];
    const nodes = this.getState().nodes;

    itemIds.forEach(function(itemId) {
      const node = nodes[itemId]!;
      if (node.url) {
        result.push(node.id);
      } else {
        node.children!.forEach(function(child) {
          const childNode = nodes[child]!;
          if (childNode.id && childNode.url) {
            result.push(childNode.id);
          }
        });
      }
    });

    return result;
  }

  private containsMatchingNode_(
      itemIds: Set<string>, predicate: (p1: BookmarkNode) => boolean): boolean {
    const nodes = this.getState().nodes;

    return Array.from(itemIds).some(function(id) {
      return !!nodes[id] && predicate(nodes[id]);
    });
  }

  private isSingleBookmark_(itemIds: Set<string>): boolean {
    return itemIds.size === 1 &&
        this.containsMatchingNode_(itemIds, function(node) {
          return !!node.url;
        });
  }

  private isFolder_(itemIds: Set<string>): boolean {
    return itemIds.size === 1 &&
        this.containsMatchingNode_(itemIds, node => !node.url);
  }

  protected getCommandLabel_(command: Command): string {
    // Handle non-pluralized strings first.
    let label = null;
    switch (command) {
      case Command.EDIT:
        if (this.menuIds_.size !== 1) {
          return '';
        }

        const id = Array.from(this.menuIds_)[0]!;
        const itemUrl = this.getState().nodes[id]!.url;
        label = itemUrl ? 'menuEdit' : 'menuRename';
        break;
      case Command.CUT:
        label = 'menuCut';
        break;
      case Command.COPY:
        label = 'menuCopy';
        break;
      case Command.PASTE:
        label = 'menuPaste';
        break;
      case Command.DELETE:
        label = 'menuDelete';
        break;
      case Command.SHOW_IN_FOLDER:
        label = 'menuShowInFolder';
        break;
      case Command.SORT:
        label = 'menuSort';
        break;
      case Command.ADD_BOOKMARK:
        label = 'menuAddBookmark';
        break;
      case Command.ADD_FOLDER:
        label = 'menuAddFolder';
        break;
      case Command.IMPORT:
        label = 'menuImport';
        break;
      case Command.EXPORT:
        label = 'menuExport';
        break;
      case Command.HELP_CENTER:
        label = 'menuHelpCenter';
        break;
      case Command.OPEN_SPLIT_VIEW:
        label = 'menuOpenSplitView';
        break;
    }
    if (label !== null) {
      return loadTimeData.getString(label);
    }

    // Handle pluralized strings.
    switch (command) {
      case Command.OPEN_NEW_TAB:
        return this.getPluralizedOpenAllString_(
            'menuOpenAllNewTab', 'menuOpenNewTab',
            'menuOpenAllNewTabWithCount');
      case Command.OPEN_NEW_WINDOW:
        return this.getPluralizedOpenAllString_(
            'menuOpenAllNewWindow', 'menuOpenNewWindow',
            'menuOpenAllNewWindowWithCount');
      case Command.OPEN_INCOGNITO:
        return this.getPluralizedOpenAllString_(
            'menuOpenAllIncognito', 'menuOpenIncognito',
            'menuOpenAllIncognitoWithCount');
      case Command.OPEN_NEW_GROUP:
        return this.getPluralizedOpenAllString_(
            'menuOpenAllNewTabGroup', 'menuOpenNewTabGroup',
            'menuOpenAllNewTabGroupWithCount');
    }

    assertNotReached();
  }

  private getPluralizedOpenAllString_(
      case0: string, case1: string, caseOther: string): string {
    const multipleNodes = this.menuIds_.size > 1 ||
        this.containsMatchingNode_(this.menuIds_, node => !node.url);

    const ids = this.expandIds_(this.menuIds_);
    if (ids.length === 0) {
      return loadTimeData.getStringF(case0, ids.length);
    }

    if (ids.length === 1 && !multipleNodes) {
      return loadTimeData.getString(case1);
    }

    return loadTimeData.getStringF(caseOther, ids.length);
  }

  protected computeMenuCommands_(): Command[] {
    switch (this.menuSource_) {
      case MenuSource.ITEM:
      case MenuSource.TREE:
        const commands = [
          Command.EDIT,
          Command.SHOW_IN_FOLDER,
          Command.DELETE,
          // <hr>
          Command.CUT,
          Command.COPY,
          Command.PASTE,
          // <hr>
          Command.OPEN_INCOGNITO,
          Command.OPEN_NEW_GROUP,
          Command.OPEN_NEW_TAB,
          Command.OPEN_NEW_WINDOW,
        ];
        if (loadTimeData.getBoolean('splitViewEnabled')) {
          commands.push(Command.OPEN_SPLIT_VIEW);
        }
        return commands;
      case MenuSource.TOOLBAR:
        return [
          Command.SORT,
          // <hr>
          Command.ADD_BOOKMARK,
          Command.ADD_FOLDER,
          // <hr>
          Command.IMPORT,
          Command.EXPORT,
          // <hr>
          Command.HELP_CENTER,
        ];
      case MenuSource.LIST:
        return [
          Command.ADD_BOOKMARK,
          Command.ADD_FOLDER,
        ];
      case MenuSource.NONE:
        return [];
    }
    assertNotReached();
  }

  protected showDividerAfter_(command: Command): boolean {
    switch (command) {
      case Command.SORT:
      case Command.ADD_FOLDER:
      case Command.EXPORT:
        return this.menuSource_ === MenuSource.TOOLBAR;
      case Command.DELETE:
        return this.globalCanEdit_;
      case Command.PASTE:
        return this.globalCanEdit_ || this.isSingleBookmark_(this.menuIds_);
    }
    return false;
  }

  private recordCommandHistogram_(
      itemIds: Set<string>, histogram: string, command: number) {
    if (command === Command.OPEN) {
      command =
          this.isFolder_(itemIds) ? Command.OPEN_FOLDER : Command.OPEN_BOOKMARK;
    }

    this.browserProxy_.recordInHistogram(histogram, command, Command.MAX_VALUE);
  }

  /**
   * Show a toast with a bookmark |title| inserted into a label, with the
   * title ellipsised if necessary.
   */
  private async showTitleToast_(
      labelPromise: Promise<string>, title: string,
      canUndo: boolean): Promise<void> {
    const label = await labelPromise;
    const pieces =
        loadTimeData.getSubstitutedStringPieces(label, title).map(function(p) {
          // Make the bookmark name collapsible.
          const result =
              p as {value: string, arg: string, collapsible: boolean};
          result.collapsible = !!p.arg;
          return result;
        });
    getToastManager().showForStringPieces(pieces, /*hideSlotted*/ !canUndo);
  }

  ////////////////////////////////////////////////////////////////////////////
  // Event handlers:

  private async onOpenCommandMenu_(
      e: CustomEvent<OpenCommandMenuDetail>): Promise<void> {
    this.isActiveTabInSplit_ =
        await BookmarkManagerApiProxyImpl.getInstance().isActiveTabInSplit();
    if (e.detail.targetId) {
      this.canPaste_ = await BookmarkManagerApiProxyImpl.getInstance().canPaste(
          e.detail.targetId);
    }
    if (e.detail.targetElement) {
      this.openCommandMenuAtElement(e.detail.targetElement, e.detail.source);
    } else {
      this.openCommandMenuAtPosition(e.detail.x!, e.detail.y!, e.detail.source);
    }
  }

  protected onCommandClick_(e: Event) {
    assert(this.menuIds_);
    this.handle(
        Number((e.currentTarget as HTMLElement).dataset['command']) as Command,
        this.menuIds_);
    this.closeCommandMenu();
  }

  private onKeydown_(e: KeyboardEvent) {
    const path = e.composedPath();
    if ((path[0] as HTMLElement).tagName === 'INPUT') {
      return;
    }
    if ((e.target === document.body ||
         path.some(
             el => (el as HTMLElement).tagName === 'BOOKMARKS-TOOLBAR')) &&
        !DialogFocusManager.getInstance().hasOpenDialog()) {
      this.handleKeyEvent(e, this.getState().selection.items);
    }
  }

  /**
   * Close the menu on mousedown so clicks can propagate to the underlying UI.
   * This allows the user to right click the list while a context menu is
   * showing and get another context menu.
   */
  protected onMenuMousedown_(e: Event): void {
    if ((e.composedPath()[0] as HTMLElement).tagName !== 'DIALOG') {
      return;
    }

    this.closeCommandMenu();
  }

  protected onOpenCancelClick_() {
    this.ensureOpenDialog_().then(dialog => dialog.cancel());
  }

  protected onOpenConfirmClick_() {
    assert(this.confirmOpenCallback_);
    this.confirmOpenCallback_();
    this.ensureOpenDialog_().then(dialog => dialog.close());
  }

  static getInstance(): BookmarksCommandManagerElement {
    assert(instance);
    return instance;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'bookmarks-command-manager': BookmarksCommandManagerElement;
  }
}

customElements.define(
    BookmarksCommandManagerElement.is, BookmarksCommandManagerElement);
