// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './shared_style.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {changeFolderOpen, selectFolder} from './actions.js';
import {CommandManager} from './command_manager.js';
import {FOLDER_OPEN_BY_DEFAULT_DEPTH, MenuSource, ROOT_NODE_ID} from './constants.js';
import {StoreClient} from './store_client.js';
import {BookmarkNode, BookmarksPageState} from './types.js';
import {hasChildFolders, isShowingSearch} from './util.js';

Polymer({
  is: 'bookmarks-folder-node',

  _template: html`{__html_template__}`,

  behaviors: [
    StoreClient,
  ],

  properties: {
    itemId: {
      type: String,
      observer: 'updateFromStore',
    },

    depth: {
      type: Number,
      observer: 'depthChanged_',
    },

    isOpen: {
      type: Boolean,
      computed: 'computeIsOpen_(openState_, depth)',
    },

    /** @type {BookmarkNode} */
    item_: Object,

    /** @private {?boolean} */
    openState_: Boolean,

    /** @private */
    selectedFolder_: String,

    /** @private */
    searchActive_: Boolean,

    /** @private */
    isSelectedFolder_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      computed: 'computeIsSelected_(itemId, selectedFolder_, searchActive_)'
    },

    /** @private */
    hasChildFolder_: {
      type: Boolean,
      computed: 'computeHasChildFolder_(item_.children)',
    },
  },

  listeners: {
    'keydown': 'onKeydown_',
  },

  observers: [
    'updateAriaExpanded_(hasChildFolder_, isOpen)',
    'scrollIntoViewIfNeeded_(isSelectedFolder_)',
  ],

  /** @override */
  attached() {
    this.watch('item_', state => {
      return /** @type {!BookmarksPageState} */ (state).nodes[this.itemId];
    });
    this.watch('openState_', state => {
      const bookmarksState = /** @type {!BookmarksPageState} */ (state);
      return bookmarksState.folderOpenState.has(this.itemId) ?
          bookmarksState.folderOpenState.get(this.itemId) :
          null;
    });
    this.watch('selectedFolder_', state => {
      return /** @type {!BookmarksPageState} */ (state).selectedFolder;
    });
    this.watch('searchActive_', state => {
      return isShowingSearch(/** @type {!BookmarksPageState} */ (state));
    });

    this.updateFromStore();
  },

  /**
   * @param {boolean} isSelectedFolder
   * @return {string}
   * @private
   */
  getContainerClass_(isSelectedFolder) {
    return isSelectedFolder ? 'selected' : '';
  },

  /** @return {!HTMLElement} */
  getFocusTarget() {
    return /** @type {!HTMLDivElement} */ (this.$.container);
  },

  /** @return {HTMLElement} */
  getDropTarget() {
    return /** @type {!HTMLDivElement} */ (this.$.container);
  },

  /**
   * @private
   * @param {!Event} e
   */
  onKeydown_(e) {
    let yDirection = 0;
    let xDirection = 0;
    let handled = true;
    if (e.key === 'ArrowUp') {
      yDirection = -1;
    } else if (e.key === 'ArrowDown') {
      yDirection = 1;
    } else if (e.key === 'ArrowLeft') {
      xDirection = -1;
    } else if (e.key === 'ArrowRight') {
      xDirection = 1;
    } else if (e.key === ' ') {
      this.selectFolder_();
    } else {
      handled = false;
    }

    if (this.getComputedStyleValue('direction') === 'rtl') {
      xDirection *= -1;
    }

    this.changeKeyboardSelection_(
        xDirection, yDirection, this.root.activeElement);

    if (!handled) {
      handled = CommandManager.getInstance().handleKeyEvent(
          e, new Set([this.itemId]));
    }

    if (!handled) {
      return;
    }

    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * @private
   * @param {number} xDirection
   * @param {number} yDirection
   * @param {!HTMLElement} currentFocus
   */
  changeKeyboardSelection_(xDirection, yDirection, currentFocus) {
    let newFocusFolderNode = null;
    const isChildFolderNodeFocused =
        currentFocus && currentFocus.tagName === 'BOOKMARKS-FOLDER-NODE';

    if (xDirection === 1) {
      // The right arrow opens a folder if closed and goes to the first child
      // otherwise.
      if (this.hasChildFolder_) {
        if (!this.isOpen) {
          this.dispatch(changeFolderOpen(this.item_.id, true));
        } else {
          yDirection = 1;
        }
      }
    } else if (xDirection === -1) {
      // The left arrow closes a folder if open and goes to the parent
      // otherwise.
      if (this.hasChildFolder_ && this.isOpen) {
        this.dispatch(changeFolderOpen(this.item_.id, false));
      } else {
        const parentFolderNode = this.getParentFolderNode_();
        if (parentFolderNode.itemId !== ROOT_NODE_ID) {
          parentFolderNode.getFocusTarget().focus();
        }
      }
    }

    if (!yDirection) {
      return;
    }

    // The current node's successor is its first child when open.
    if (!isChildFolderNodeFocused && yDirection === 1 && this.isOpen) {
      const children = this.getChildFolderNodes_();
      if (children.length) {
        newFocusFolderNode = children[0];
      }
    }

    if (isChildFolderNodeFocused) {
      // Get the next child folder node if a child is focused.
      if (!newFocusFolderNode) {
        newFocusFolderNode = this.getNextChild_(
            yDirection === -1,
            /** @type {!BookmarksFolderNodeElement} */ (currentFocus));
      }

      // The first child's predecessor is this node.
      if (!newFocusFolderNode && yDirection === -1) {
        newFocusFolderNode = this;
      }
    }

    // If there is no newly focused node, allow the parent to handle the change.
    if (!newFocusFolderNode) {
      if (this.itemId !== ROOT_NODE_ID) {
        this.getParentFolderNode_().changeKeyboardSelection_(
            0, yDirection, this);
      }

      return;
    }

    // The root node is not navigable.
    if (newFocusFolderNode.itemId !== ROOT_NODE_ID) {
      newFocusFolderNode.getFocusTarget().focus();
    }
  },

  /**
   * Returns the next or previous visible bookmark node relative to |child|.
   * @private
   * @param {boolean} reverse
   * @param {!BookmarksFolderNodeElement} child
   * @return {BookmarksFolderNodeElement|null} Returns null if there is no child
   *     before/after |child|.
   */
  getNextChild_(reverse, child) {
    let newFocus = null;
    const children = this.getChildFolderNodes_();

    const index = children.indexOf(child);
    assert(index !== -1);
    if (reverse) {
      // A child node's predecessor is either the previous child's last visible
      // descendant, or this node, which is its immediate parent.
      newFocus =
          index === 0 ? null : children[index - 1].getLastVisibleDescendant_();
    } else if (index < children.length - 1) {
      // A successor to a child is the next child.
      newFocus = children[index + 1];
    }

    return newFocus;
  },

  /**
   * Returns the immediate parent folder node, or null if there is none.
   * @private
   * @return {BookmarksFolderNodeElement|null}
   */
  getParentFolderNode_() {
    let parentFolderNode = this.parentNode;
    while (parentFolderNode &&
           parentFolderNode.tagName !== 'BOOKMARKS-FOLDER-NODE') {
      parentFolderNode = parentFolderNode.parentNode || parentFolderNode.host;
    }
    return parentFolderNode || null;
  },

  /**
   * @private
   * @return {BookmarksFolderNodeElement}
   */
  getLastVisibleDescendant_() {
    const children = this.getChildFolderNodes_();
    if (!this.isOpen || children.length === 0) {
      return this;
    }

    return children.pop().getLastVisibleDescendant_();
  },

  /** @private */
  selectFolder_() {
    if (!this.isSelectedFolder_) {
      this.dispatch(selectFolder(this.itemId, this.getState().nodes));
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onContextMenu_(e) {
    e.preventDefault();
    this.selectFolder_();
    CommandManager.getInstance().openCommandMenuAtPosition(
        e.clientX, e.clientY, MenuSource.TREE, new Set([this.itemId]));
  },

  /**
   * @private
   * @return {!Array<!BookmarksFolderNodeElement>}
   */
  getChildFolderNodes_() {
    return Array.from(this.root.querySelectorAll('bookmarks-folder-node'));
  },

  /**
   * Toggles whether the folder is open.
   * @private
   * @param {!Event} e
   */
  toggleFolder_(e) {
    this.dispatch(changeFolderOpen(this.itemId, !this.isOpen));
    e.stopPropagation();
  },

  /**
   * @private
   * @param {!Event} e
   */
  preventDefault_(e) {
    e.preventDefault();
  },

  /**
   * @private
   * @param {string} itemId
   * @param {string} selectedFolder
   * @return {boolean}
   */
  computeIsSelected_(itemId, selectedFolder, searchActive) {
    return itemId === selectedFolder && !searchActive;
  },

  /**
   * @private
   * @return {boolean}
   */
  computeHasChildFolder_() {
    return hasChildFolders(this.itemId, this.getState().nodes);
  },

  /** @private */
  depthChanged_() {
    this.style.setProperty('--node-depth', String(this.depth));
    if (this.depth === -1) {
      this.$.descendants.removeAttribute('role');
    }
  },

  /**
   * @private
   * @return {number}
   */
  getChildDepth_() {
    return this.depth + 1;
  },

  /**
   * @param {string} itemId
   * @private
   * @return {boolean}
   */
  isFolder_(itemId) {
    return !this.getState().nodes[itemId].url;
  },

  /**
   * @private
   * @return {boolean}
   */
  isRootFolder_() {
    return this.itemId === ROOT_NODE_ID;
  },

  /**
   * @private
   * @return {string}
   */
  getTabIndex_() {
    // This returns a tab index of 0 for the cached selected folder when the
    // search is active, even though this node is not technically selected. This
    // allows the sidebar to be focusable during a search.
    return this.selectedFolder_ === this.itemId ? '0' : '-1';
  },

  /**
   * Sets the 'aria-expanded' accessibility on nodes which need it. Note that
   * aria-expanded="false" is different to having the attribute be undefined.
   * @param {boolean} hasChildFolder
   * @param {boolean} isOpen
   * @private
   */
  updateAriaExpanded_(hasChildFolder, isOpen) {
    if (hasChildFolder) {
      this.getFocusTarget().setAttribute('aria-expanded', String(isOpen));
    } else {
      this.getFocusTarget().removeAttribute('aria-expanded');
    }
  },

  /**
   * Scrolls the folder node into view when the folder is selected.
   * @private
   */
  scrollIntoViewIfNeeded_() {
    if (!this.isSelectedFolder_) {
      return;
    }

    this.async(() => this.$.container.scrollIntoViewIfNeeded());
  },

  /**
   * @param {?boolean} openState
   * @param {number} depth
   * @return {boolean}
   */
  computeIsOpen_(openState, depth) {
    return openState != null ? openState :
                               depth <= FOLDER_OPEN_BY_DEFAULT_DEPTH;
  },
});
