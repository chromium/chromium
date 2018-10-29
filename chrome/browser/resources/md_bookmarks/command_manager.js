// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Element which shows context menus and handles keyboard
 * shortcuts.
 */
cr.define('bookmarks', function() {

  const CommandManager = Polymer({
    is: 'bookmarks-command-manager',

    behaviors: [
      bookmarks.StoreClient,
    ],

    properties: {
      /** @private {!Array<Command>} */
      menuCommands_: {
        type: Array,
        computed: 'computeMenuCommands_(menuSource_)',
      },

      /** @private {Set<string>} */
      menuIds_: Object,

      /** @private */
      hasAnySublabel_: {
        type: Boolean,
        reflectToAttribute: true,
        computed: 'computeHasAnySublabel_(menuCommands_, menuIds_)',
      },

      /**
       * Indicates where the context menu was opened from. Will be NONE if
       * menu is not open, indicating that commands are from keyboard shortcuts
       * or elsewhere in the UI.
       * @private {MenuSource}
       */
      menuSource_: {
        type: Number,
        value: MenuSource.NONE,
      },

      /** @private */
      globalCanEdit_: Boolean,
    },

    /** @private {?Function} */
    confirmOpenCallback_: null,

    attached: function() {
      assert(CommandManager.instance_ == null);
      CommandManager.instance_ = this;

      this.watch('globalCanEdit_', function(state) {
        return state.prefs.canEdit;
      });
      this.updateFromStore();

      /** @private {!Map<Command, cr.ui.KeyboardShortcutList>} */
      this.shortcuts_ = new Map();

      this.addShortcut_(Command.EDIT, 'F2', 'Enter');
      this.addShortcut_(Command.DELETE, 'Delete', 'Delete Backspace');

      this.addShortcut_(Command.OPEN, 'Enter', 'Meta|o');
      this.addShortcut_(Command.OPEN_NEW_TAB, 'Ctrl|Enter', 'Meta|Enter');
      this.addShortcut_(Command.OPEN_NEW_WINDOW, 'Shift|Enter');

      // Note: the undo shortcut is also defined in md_bookmarks_ui.cc
      // TODO(b/893033): de-duplicate shortcut by moving all shortcut
      // definitions from JS to C++.
      this.addShortcut_(Command.UNDO, 'Ctrl|z', 'Meta|z');
      this.addShortcut_(Command.REDO, 'Ctrl|y Ctrl|Shift|Z', 'Meta|Shift|Z');

      this.addShortcut_(Command.SELECT_ALL, 'Ctrl|a', 'Meta|a');
      this.addShortcut_(Command.DESELECT_ALL, 'Escape');

      this.addShortcut_(Command.CUT, 'Ctrl|x', 'Meta|x');
      this.addShortcut_(Command.COPY, 'Ctrl|c', 'Meta|c');
      this.addShortcut_(Command.PASTE, 'Ctrl|v', 'Meta|v');

      /** @private {!Map<string, Function>} */
      this.boundListeners_ = new Map();

      const addDocumentListener = (eventName, handler) => {
        assert(!this.boundListeners_.has(eventName));
        const boundListener = handler.bind(this);
        this.boundListeners_.set(eventName, boundListener);
        document.addEventListener(eventName, boundListener);
      };
      addDocumentListener('open-command-menu', this.onOpenCommandMenu_);
      addDocumentListener('keydown', this.onKeydown_);

      const addDocumentListenerForCommand = (eventName, command) => {
        addDocumentListener(eventName, (e) => {
          if (e.path[0].tagName == 'INPUT')
            return;

          const items = this.getState().selection.items;
          if (this.canExecute(command, items))
            this.handle(command, items);
        });
      };
      addDocumentListenerForCommand('command-undo', Command.UNDO);
      addDocumentListenerForCommand('cut', Command.CUT);
      addDocumentListenerForCommand('copy', Command.COPY);
      addDocumentListenerForCommand('paste', Command.PASTE);
    },

    detached: function() {
      CommandManager.instance_ = null;
      this.boundListeners_.forEach(
          (handler, eventName) =>
              document.removeEventListener(eventName, handler));
    },

    /**
     * Display the command context menu at (|x|, |y|) in window co-ordinates.
     * Commands will execute on |items| if given, or on the currently selected
     * items.
     * @param {number} x
     * @param {number} y
     * @param {MenuSource} source
     * @param {Set<string>=} items
     */
    openCommandMenuAtPosition: function(x, y, source, items) {
      this.menuSource_ = source;
      this.menuIds_ = items || this.getState().selection.items;

      const dropdown =
          /** @type {!CrActionMenuElement} */ (this.$.dropdown.get());
      // Ensure that the menu is fully rendered before trying to position it.
      Polymer.dom.flush();
      bookmarks.DialogFocusManager.getInstance().showDialog(
          dropdown.getDialog(), function() {
            dropdown.showAtPosition({top: y, left: x});
          });
    },

    /**
     * Display the command context menu positioned to cover the |target|
     * element. Commands will execute on the currently selected items.
     * @param {!Element} target
     * @param {MenuSource} source
     */
    openCommandMenuAtElement: function(target, source) {
      this.menuSource_ = source;
      this.menuIds_ = this.getState().selection.items;

      const dropdown =
          /** @type {!CrActionMenuElement} */ (this.$.dropdown.get());
      // Ensure that the menu is fully rendered before trying to position it.
      Polymer.dom.flush();
      bookmarks.DialogFocusManager.getInstance().showDialog(
          dropdown.getDialog(), function() {
            dropdown.showAt(target);
          });
    },

    closeCommandMenu: function() {
      this.menuIds_ = new Set();
      this.menuSource_ = MenuSource.NONE;
      /** @type {!CrActionMenuElement} */ (this.$.dropdown.get()).close();
    },

    ////////////////////////////////////////////////////////////////////////////
    // Command handlers:

    /**
     * Determine if the |command| can be executed with the given |itemIds|.
     * Commands which appear in the context menu should be implemented
     * separately using `isCommandVisible_` and `isCommandEnabled_`.
     * @param {Command} command
     * @param {!Set<string>} itemIds
     * @return {boolean}
     */
    canExecute: function(command, itemIds) {
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
          return itemIds.size > 0;
        case Command.CUT:
          return itemIds.size > 0 &&
              !this.containsMatchingNode_(itemIds, function(node) {
                return !bookmarks.util.canEditNode(state, node.id);
              });
        case Command.PASTE:
          return state.search.term == '' &&
              bookmarks.util.canReorderChildren(state, state.selectedFolder);
        default:
          return this.isCommandVisible_(command, itemIds) &&
              this.isCommandEnabled_(command, itemIds);
      }
    },

    /**
     * @param {Command} command
     * @param {!Set<string>} itemIds
     * @return {boolean} True if the command should be visible in the context
     *     menu.
     */
    isCommandVisible_: function(command, itemIds) {
      switch (command) {
        case Command.EDIT:
          return itemIds.size == 1 && this.globalCanEdit_;
        case Command.COPY_URL:
          return this.isSingleBookmark_(itemIds);
        case Command.DELETE:
          return itemIds.size > 0 && this.globalCanEdit_;
        case Command.SHOW_IN_FOLDER:
          return this.menuSource_ == MenuSource.ITEM && itemIds.size == 1 &&
              this.getState().search.term != '' &&
              !this.containsMatchingNode_(itemIds, function(node) {
                return !node.parentId || node.parentId == ROOT_NODE_ID;
              });
        case Command.OPEN_NEW_TAB:
        case Command.OPEN_NEW_WINDOW:
        case Command.OPEN_INCOGNITO:
          return itemIds.size > 0;
        case Command.ADD_BOOKMARK:
        case Command.ADD_FOLDER:
        case Command.SORT:
        case Command.EXPORT:
        case Command.IMPORT:
        case Command.HELP_CENTER:
          return true;
      }
      return assert(false);
    },

    /**
     * @param {Command} command
     * @param {!Set<string>} itemIds
     * @return {boolean} True if the command should be clickable in the context
     *     menu.
     */
    isCommandEnabled_: function(command, itemIds) {
      const state = this.getState();
      switch (command) {
        case Command.EDIT:
        case Command.DELETE:
          return !this.containsMatchingNode_(itemIds, function(node) {
            return !bookmarks.util.canEditNode(state, node.id);
          });
        case Command.OPEN_NEW_TAB:
        case Command.OPEN_NEW_WINDOW:
          return this.expandUrls_(itemIds).length > 0;
        case Command.OPEN_INCOGNITO:
          return this.expandUrls_(itemIds).length > 0 &&
              state.prefs.incognitoAvailability !=
              IncognitoAvailability.DISABLED;
        case Command.SORT:
          return this.canChangeList_() &&
              state.nodes[state.selectedFolder].children.length > 1;
        case Command.ADD_BOOKMARK:
        case Command.ADD_FOLDER:
          return this.canChangeList_();
        case Command.IMPORT:
          return this.globalCanEdit_;
        default:
          return true;
      }
    },

    /**
     * Returns whether the currently displayed bookmarks list can be changed.
     * @private
     * @return {boolean}
     */
    canChangeList_: function() {
      const state = this.getState();
      return state.search.term == '' &&
          bookmarks.util.canReorderChildren(state, state.selectedFolder);
    },

    /**
     * @param {Command} command
     * @param {!Set<string>} itemIds
     */
    handle: function(command, itemIds) {
      const state = this.getState();
      switch (command) {
        case Command.EDIT: {
          let id = Array.from(itemIds)[0];
          /** @type {!BookmarksEditDialogElement} */ (this.$.editDialog.get())
              .showEditDialog(state.nodes[id]);
          break;
        }
        case Command.COPY_URL:
        case Command.COPY: {
          let idList = Array.from(itemIds);
          chrome.bookmarkManagerPrivate.copy(idList, () => {
            let labelPromise;
            if (command == Command.COPY_URL) {
              labelPromise =
                  Promise.resolve(loadTimeData.getString('toastUrlCopied'));
            } else if (idList.length == 1) {
              labelPromise =
                  Promise.resolve(loadTimeData.getString('toastItemCopied'));
            } else {
              labelPromise = cr.sendWithPromise(
                  'getPluralString', 'toastItemsCopied', idList.length);
            }

            this.showTitleToast_(
                labelPromise, state.nodes[idList[0]].title, false);
          });
          break;
        }
        case Command.SHOW_IN_FOLDER: {
          let id = Array.from(itemIds)[0];
          this.dispatch(bookmarks.actions.selectFolder(
              assert(state.nodes[id].parentId), state.nodes));
          bookmarks.DialogFocusManager.getInstance().clearFocus();
          this.fire('highlight-items', [id]);
          break;
        }
        case Command.DELETE: {
          let idList = Array.from(this.minimizeDeletionSet_(itemIds));
          const title = state.nodes[idList[0]].title;
          let labelPromise;

          if (idList.length == 1) {
            labelPromise =
                Promise.resolve(loadTimeData.getString('toastItemDeleted'));
          } else {
            labelPromise = cr.sendWithPromise(
                'getPluralString', 'toastItemsDeleted', idList.length);
          }

          chrome.bookmarkManagerPrivate.removeTrees(idList, () => {
            this.showTitleToast_(labelPromise, title, true);
          });
          break;
        }
        case Command.UNDO:
          chrome.bookmarkManagerPrivate.undo();
          bookmarks.ToastManager.getInstance().hide();
          break;
        case Command.REDO:
          chrome.bookmarkManagerPrivate.redo();
          break;
        case Command.OPEN_NEW_TAB:
        case Command.OPEN_NEW_WINDOW:
        case Command.OPEN_INCOGNITO:
          this.openUrls_(this.expandUrls_(itemIds), command);
          break;
        case Command.OPEN:
          const isFolder = itemIds.size == 1 &&
              this.containsMatchingNode_(itemIds, function(node) {
                return !node.url;
              });
          if (isFolder) {
            const folderId = Array.from(itemIds)[0];
            this.dispatch(
                bookmarks.actions.selectFolder(folderId, state.nodes));
          } else {
            this.openUrls_(this.expandUrls_(itemIds), command);
          }
          break;
        case Command.SELECT_ALL:
          const displayedIds = bookmarks.util.getDisplayedList(state);
          this.dispatch(bookmarks.actions.selectAll(displayedIds, state));
          break;
        case Command.DESELECT_ALL:
          this.dispatch(bookmarks.actions.deselectItems());
          break;
        case Command.CUT:
          chrome.bookmarkManagerPrivate.cut(Array.from(itemIds));
          break;
        case Command.PASTE:
          const selectedFolder = state.selectedFolder;
          const selectedItems = state.selection.items;
          bookmarks.ApiListener.trackUpdatedItems();
          chrome.bookmarkManagerPrivate.paste(
              selectedFolder, Array.from(selectedItems),
              bookmarks.ApiListener.highlightUpdatedItems);
          break;
        case Command.SORT:
          chrome.bookmarkManagerPrivate.sortChildren(
              assert(state.selectedFolder));
          bookmarks.ToastManager.getInstance().show(
              loadTimeData.getString('toastFolderSorted'), true);
          break;
        case Command.ADD_BOOKMARK:
          /** @type {!BookmarksEditDialogElement} */ (this.$.editDialog.get())
              .showAddDialog(false, assert(state.selectedFolder));
          break;
        case Command.ADD_FOLDER:
          /** @type {!BookmarksEditDialogElement} */ (this.$.editDialog.get())
              .showAddDialog(true, assert(state.selectedFolder));
          break;
        case Command.IMPORT:
          chrome.bookmarks.import();
          break;
        case Command.EXPORT:
          chrome.bookmarks.export();
          break;
        case Command.HELP_CENTER:
          window.open('https://support.google.com/chrome/?p=bookmarks');
          break;
        default:
          assert(false);
      }

      bookmarks.util.recordEnumHistogram(
          'BookmarkManager.CommandExecuted', command, Command.MAX_VALUE);
    },

    /**
     * @param {!Event} e
     * @param {!Set<string>} itemIds
     * @return {boolean} True if the event was handled, triggering a keyboard
     *     shortcut.
     */
    handleKeyEvent: function(e, itemIds) {
      for (const commandTuple of this.shortcuts_) {
        const command = /** @type {Command} */ (commandTuple[0]);
        const shortcut =
            /** @type {cr.ui.KeyboardShortcutList} */ (commandTuple[1]);
        if (shortcut.matchesEvent(e) && this.canExecute(command, itemIds)) {
          this.handle(command, itemIds);

          bookmarks.util.recordEnumHistogram(
              'BookmarkManager.CommandExecutedFromKeyboard', command,
              Command.MAX_VALUE);
          e.stopPropagation();
          e.preventDefault();
          return true;
        }
      }

      return false;
    },

    ////////////////////////////////////////////////////////////////////////////
    // Private functions:

    /**
     * Register a keyboard shortcut for a command.
     * @param {Command} command Command that the shortcut will trigger.
     * @param {string} shortcut Keyboard shortcut, using the syntax of
     *     cr/ui/command.js.
     * @param {string=} macShortcut If set, enables a replacement shortcut for
     *     Mac.
     */
    addShortcut_: function(command, shortcut, macShortcut) {
      shortcut = (cr.isMac && macShortcut) ? macShortcut : shortcut;
      this.shortcuts_.set(command, new cr.ui.KeyboardShortcutList(shortcut));
    },

    /**
     * Minimize the set of |itemIds| by removing any node which has an ancestor
     * node already in the set. This ensures that instead of trying to delete
     * both a node and its descendant, we will only try to delete the topmost
     * node, preventing an error in the bookmarkManagerPrivate.removeTrees API
     * call.
     * @param {!Set<string>} itemIds
     * @return {!Set<string>}
     */
    minimizeDeletionSet_: function(itemIds) {
      const minimizedSet = new Set();
      const nodes = this.getState().nodes;
      itemIds.forEach(function(itemId) {
        let currentId = itemId;
        while (currentId != ROOT_NODE_ID) {
          currentId = assert(nodes[currentId].parentId);
          if (itemIds.has(currentId))
            return;
        }
        minimizedSet.add(itemId);
      });
      return minimizedSet;
    },

    /**
     * Open the given |urls| in response to a |command|. May show a confirmation
     * dialog before opening large numbers of URLs.
     * @param {!Array<string>} urls
     * @param {Command} command
     * @private
     */
    openUrls_: function(urls, command) {
      assert(
          command == Command.OPEN || command == Command.OPEN_NEW_TAB ||
          command == Command.OPEN_NEW_WINDOW ||
          command == Command.OPEN_INCOGNITO);

      if (urls.length == 0)
        return;

      const openUrlsCallback = function() {
        const incognito = command == Command.OPEN_INCOGNITO;
        if (command == Command.OPEN_NEW_WINDOW || incognito) {
          chrome.windows.create({url: urls, incognito: incognito});
        } else {
          if (command == Command.OPEN)
            chrome.tabs.create({url: urls.shift(), active: true});
          urls.forEach(function(url) {
            chrome.tabs.create({url: url, active: false});
          });
        }
      };

      if (urls.length <= OPEN_CONFIRMATION_LIMIT) {
        openUrlsCallback();
        return;
      }

      this.confirmOpenCallback_ = openUrlsCallback;
      const dialog = this.$.openDialog.get();
      dialog.querySelector('[slot=body]').textContent =
          loadTimeData.getStringF('openDialogBody', urls.length);

      bookmarks.DialogFocusManager.getInstance().showDialog(
          this.$.openDialog.get());
    },

    /**
     * Returns all URLs in the given set of nodes and their immediate children.
     * Note that these will be ordered by insertion order into the |itemIds|
     * set, and that it is possible to duplicate a URL by passing in both the
     * parent ID and child ID.
     * @param {!Set<string>} itemIds
     * @return {!Array<string>}
     * @private
     */
    expandUrls_: function(itemIds) {
      const urls = [];
      const nodes = this.getState().nodes;

      itemIds.forEach(function(id) {
        const node = nodes[id];
        if (node.url) {
          urls.push(node.url);
        } else {
          node.children.forEach(function(childId) {
            const childNode = nodes[childId];
            if (childNode.url)
              urls.push(childNode.url);
          });
        }
      });

      return urls;
    },

    /**
     * @param {!Set<string>} itemIds
     * @param {function(BookmarkNode):boolean} predicate
     * @return {boolean} True if any node in |itemIds| returns true for
     *     |predicate|.
     */
    containsMatchingNode_: function(itemIds, predicate) {
      const nodes = this.getState().nodes;

      return Array.from(itemIds).some(function(id) {
        return predicate(nodes[id]);
      });
    },

    /**
     * @param {!Set<string>} itemIds
     * @return {boolean} True if |itemIds| is a single bookmark (non-folder)
     *     node.
     */
    isSingleBookmark_: function(itemIds) {
      return itemIds.size == 1 &&
          this.containsMatchingNode_(itemIds, function(node) {
            return !!node.url;
          });
    },

    /**
     * @param {Command} command
     * @return {string}
     * @private
     */
    getCommandLabel_: function(command) {
      const multipleNodes = this.menuIds_.size > 1 ||
          this.containsMatchingNode_(this.menuIds_, function(node) {
            return !node.url;
          });
      let label;
      switch (command) {
        case Command.EDIT:
          if (this.menuIds_.size != 1)
            return '';

          const id = Array.from(this.menuIds_)[0];
          const itemUrl = this.getState().nodes[id].url;
          label = itemUrl ? 'menuEdit' : 'menuRename';
          break;
        case Command.COPY_URL:
          label = 'menuCopyURL';
          break;
        case Command.DELETE:
          label = 'menuDelete';
          break;
        case Command.SHOW_IN_FOLDER:
          label = 'menuShowInFolder';
          break;
        case Command.OPEN_NEW_TAB:
          label = multipleNodes ? 'menuOpenAllNewTab' : 'menuOpenNewTab';
          break;
        case Command.OPEN_NEW_WINDOW:
          label = multipleNodes ? 'menuOpenAllNewWindow' : 'menuOpenNewWindow';
          break;
        case Command.OPEN_INCOGNITO:
          label = multipleNodes ? 'menuOpenAllIncognito' : 'menuOpenIncognito';
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
      }
      assert(label);

      return loadTimeData.getString(assert(label));
    },

    /**
     * @param {Command} command
     * @return {string}
     * @private
     */
    getCommandSublabel_: function(command) {
      const multipleNodes = this.menuIds_.size > 1 ||
          this.containsMatchingNode_(this.menuIds_, function(node) {
            return !node.url;
          });
      switch (command) {
        case Command.OPEN_NEW_TAB:
          const urls = this.expandUrls_(this.menuIds_);
          return multipleNodes && urls.length > 0 ? String(urls.length) : '';
        default:
          return '';
      }
    },

    /** @private */
    computeMenuCommands_: function() {
      switch (this.menuSource_) {
        case MenuSource.ITEM:
        case MenuSource.TREE:
          return [
            Command.EDIT,
            Command.COPY_URL,
            Command.SHOW_IN_FOLDER,
            Command.DELETE,
            // <hr>
            Command.OPEN_NEW_TAB,
            Command.OPEN_NEW_WINDOW,
            Command.OPEN_INCOGNITO,
          ];
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
      assert(false);
    },

    /**
     * @return {boolean}
     * @private
     */
    computeHasAnySublabel_: function() {
      if (this.menuIds_ == undefined || this.menuCommands_ == undefined)
        return false;

      return this.menuCommands_.some(
          (command) => this.getCommandSublabel_(command) != '');
    },

    /**
     * @param {Command} command
     * @return {boolean}
     * @private
     */
    showDividerAfter_: function(command, itemIds) {
      return ((command == Command.SORT || command == Command.ADD_FOLDER ||
               command == Command.EXPORT) &&
              this.menuSource_ == MenuSource.TOOLBAR) ||
          (command == Command.DELETE &&
           (this.globalCanEdit_ || this.isSingleBookmark_(itemIds)));
    },

    /**
     * Show a toast with a bookmark |title| inserted into a label, with the
     * title ellipsised if necessary.
     * @param {!Promise<string>} labelPromise Promise which resolves with the
     *    label for the toast.
     * @param {string} title Bookmark title to insert.
     * @param {boolean} canUndo If true, shows an undo button in the toast.
     * @private
     */
    showTitleToast_: function(labelPromise, title, canUndo) {
      labelPromise.then(function(label) {
        const pieces = loadTimeData.getSubstitutedStringPieces(label, title)
                           .map(function(p) {
                             // Make the bookmark name collapsible.
                             p.collapsible = !!p.arg;
                             return p;
                           });

        bookmarks.ToastManager.getInstance().showForStringPieces(
            pieces, canUndo);
      });
    },

    ////////////////////////////////////////////////////////////////////////////
    // Event handlers:

    /**
     * @param {Event} e
     * @private
     */
    onOpenCommandMenu_: function(e) {
      if (e.detail.targetElement) {
        this.openCommandMenuAtElement(e.detail.targetElement, e.detail.source);
      } else {
        this.openCommandMenuAtPosition(e.detail.x, e.detail.y, e.detail.source);
      }
      bookmarks.util.recordEnumHistogram(
          'BookmarkManager.CommandMenuOpened', e.detail.source,
          MenuSource.NUM_VALUES);
    },

    /**
     * @param {Event} e
     * @private
     */
    onCommandClick_: function(e) {
      this.handle(
          /** @type {Command} */ (
              Number(e.currentTarget.getAttribute('command'))),
          assert(this.menuIds_));
      this.closeCommandMenu();
    },

    /**
     * @param {!Event} e
     * @private
     */
    onKeydown_: function(e) {
      const selection = this.getState().selection.items;
      if (e.target == document.body &&
          !bookmarks.DialogFocusManager.getInstance().hasOpenDialog()) {
        this.handleKeyEvent(e, selection);
      }
    },

    /**
     * Close the menu on mousedown so clicks can propagate to the underlying UI.
     * This allows the user to right click the list while a context menu is
     * showing and get another context menu.
     * @param {Event} e
     * @private
     */
    onMenuMousedown_: function(e) {
      if (e.path[0].tagName != 'DIALOG')
        return;

      this.closeCommandMenu();
    },

    /** @private */
    onOpenCancelTap_: function() {
      this.$.openDialog.get().cancel();
    },

    /** @private */
    onOpenConfirmTap_: function() {
      this.confirmOpenCallback_();
      this.$.openDialog.get().close();
    },
  });

  /** @private {bookmarks.CommandManager} */
  CommandManager.instance_ = null;

  /** @return {!bookmarks.CommandManager} */
  CommandManager.getInstance = function() {
    return assert(CommandManager.instance_);
  };

  return {
    CommandManager: CommandManager,
  };
});
