// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_nav_menu_item_style.css.js';
import 'chrome://resources/cr_elements/cr_ripple/cr_ripple.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './shared_style.css.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {isRTL} from 'chrome://resources/js/util.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {changeFolderOpen, selectFolder} from './actions.js';
import {BookmarksCommandManagerElement} from './command_manager.js';
import {FOLDER_OPEN_BY_DEFAULT_DEPTH, MenuSource, ROOT_NODE_ID} from './constants.js';
import {getTemplate} from './folder_node.html.js';
import {StoreClientMixin} from './store_client_mixin.js';
import type {BookmarkNode} from './types.js';
import {hasChildFolders, isShowingSearch} from './util.js';

const BookmarksFolderNodeElementBase = StoreClientMixin(PolymerElement);

export interface BookmarksFolderNodeElement {
  $: {
    container: HTMLElement,
    descendants: HTMLElement,
  };
}

export class BookmarksFolderNodeElement extends BookmarksFolderNodeElementBase {
  static get is() {
    return 'bookmarks-folder-node';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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

      item_: Object,

      openState_: Boolean,

      selectedFolder_: String,

      searchActive_: Boolean,

      isSelectedFolder_: {
        type: Boolean,
        reflectToAttribute: true,
        computed: 'computeIsSelected_(itemId, selectedFolder_, searchActive_)',
      },

      hasChildFolder_: {
        type: Boolean,
        computed: 'computeHasChildFolder_(item_.children)',
      },
    };
  }

  depth: number;
  isOpen: boolean;
  itemId: string;
  private item_: BookmarkNode;
  private openState_: boolean;
  private selectedFolder_: string;
  private searchActive_: boolean;
  private isSelectedFolder_: boolean = false;
  private hasChildFolder_: boolean;

  static get observers() {
    return [
      'updateAriaExpanded_(hasChildFolder_, isOpen)',
      'scrollIntoViewIfNeeded_(isSelectedFolder_)',
    ];
  }

  override ready() {
    super.ready();

    this.addEventListener('keydown', e => this.onKeydown_(e));
  }

  /** @override */
  override connectedCallback() {
    super.connectedCallback();
    this.watch('item_', state => {
      return state.nodes[this.itemId];
    });
    this.watch('openState_', state => {
      return state.folderOpenState.has(this.itemId) ?
          state.folderOpenState.get(this.itemId) :
          null;
    });
    this.watch('selectedFolder_', state => state.selectedFolder);
    this.watch('searchActive_', state => {
      return isShowingSearch(state);
    });

    this.updateFromStore();
  }

  private getContainerClass_(isSelectedFolder: boolean): string {
    return isSelectedFolder ? 'selected' : '';
  }

  getFocusTarget(): HTMLElement {
    return this.$.container;
  }

  getDropTarget(): HTMLElement {
    return this.$.container;
  }

  private onKeydown_(e: KeyboardEvent) {
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

    if (isRTL()) {
      xDirection *= -1;
    }

    this.changeKeyboardSelection_(
        xDirection, yDirection, this.shadowRoot!.activeElement);

    if (!handled) {
      handled = BookmarksCommandManagerElement.getInstance().handleKeyEvent(
          e, new Set([this.itemId]));
    }

    if (!handled) {
      return;
    }

    e.preventDefault();
    e.stopPropagation();
  }

  private changeKeyboardSelection_(
      xDirection: number, yDirection: number, currentFocus: Element|null) {
    let newFocusFolderNode = null;
    const isChildFolderNodeFocused = currentFocus &&
        (currentFocus as HTMLElement)!.tagName === 'BOOKMARKS-FOLDER-NODE';

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
        const parentFolderNode = this.getParentFolderNode();
        if (parentFolderNode!.itemId !== ROOT_NODE_ID) {
          parentFolderNode!.getFocusTarget().focus();
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
        newFocusFolderNode = this.getNextChild(
            yDirection === -1, (currentFocus! as BookmarksFolderNodeElement));
      }

      // The first child's predecessor is this node.
      if (!newFocusFolderNode && yDirection === -1) {
        newFocusFolderNode = this;
      }
    }

    // If there is no newly focused node, allow the parent to handle the change.
    if (!newFocusFolderNode) {
      if (this.itemId !== ROOT_NODE_ID) {
        this.getParentFolderNode()!.changeKeyboardSelection_(
            0, yDirection, this);
      }

      return;
    }

    // The root node is not navigable.
    if (newFocusFolderNode.itemId !== ROOT_NODE_ID) {
      newFocusFolderNode.getFocusTarget().focus();
    }
  }

  /**
   * Returns the next or previous visible bookmark node relative to |child|.
   */
  getNextChild(reverse: boolean, child: BookmarksFolderNodeElement):
      BookmarksFolderNodeElement|null {
    let newFocus = null;
    const children = this.getChildFolderNodes_();

    const index = children.indexOf(child);
    assert(index !== -1);
    if (reverse) {
      // A child node's predecessor is either the previous child's last visible
      // descendant, or this node, which is its immediate parent.
      newFocus =
          index === 0 ? null : children[index - 1]!.getLastVisibleDescendant();
    } else if (index < children.length - 1) {
      // A successor to a child is the next child.
      newFocus = children[index + 1]!;
    }

    return newFocus;
  }

  /**
   * Returns the immediate parent folder node, or null if there is none.
   */
  getParentFolderNode(): BookmarksFolderNodeElement|null {
    let parentFolderNode = this.parentNode;
    while (parentFolderNode &&
           (parentFolderNode as HTMLElement).tagName !==
               'BOOKMARKS-FOLDER-NODE') {
      parentFolderNode =
          parentFolderNode.parentNode || (parentFolderNode as ShadowRoot).host;
    }
    return (parentFolderNode as BookmarksFolderNodeElement) || null;
  }

  getLastVisibleDescendant(): BookmarksFolderNodeElement {
    const children = this.getChildFolderNodes_();
    if (!this.isOpen || children.length === 0) {
      return this;
    }

    return children.pop()!.getLastVisibleDescendant();
  }

  private selectFolder_() {
    if (!this.isSelectedFolder_) {
      this.dispatch(selectFolder(this.itemId, this.getState().nodes));
    }
  }

  private onContextMenu_(e: MouseEvent) {
    e.preventDefault();
    this.selectFolder_();
    BookmarksCommandManagerElement.getInstance().openCommandMenuAtPosition(
        e.clientX, e.clientY, MenuSource.TREE, new Set([this.itemId]));
  }

  private getChildFolderNodes_(): BookmarksFolderNodeElement[] {
    return Array.from(this.shadowRoot!.querySelectorAll(
        'bookmarks-folder-node'));
  }

  /**
   * Toggles whether the folder is open.
   */
  private toggleFolder_(e: Event) {
    this.dispatch(changeFolderOpen(this.itemId, !this.isOpen));
    e.stopPropagation();
  }

  private preventDefault_(e: Event) {
    e.preventDefault();
  }

  private computeIsSelected_(
      itemId: string, selectedFolder: string, searchActive: boolean): boolean {
    return itemId === selectedFolder && !searchActive;
  }

  private computeHasChildFolder_(): boolean {
    return hasChildFolders(this.itemId, this.getState().nodes);
  }

  private depthChanged_() {
    this.style.setProperty('--node-depth', String(this.depth));
    if (this.depth === -1) {
      this.$.descendants.removeAttribute('role');
    }
  }

  private getChildDepth_(): number {
    return this.depth + 1;
  }

  private isFolder_(itemId: string): boolean {
    return !this.getState().nodes[itemId]!.url;
  }

  private isRootFolder_(): boolean {
    return this.itemId === ROOT_NODE_ID;
  }

  private getTabIndex_(): string {
    // This returns a tab index of 0 for the cached selected folder when the
    // search is active, even though this node is not technically selected. This
    // allows the sidebar to be focusable during a search.
    return this.selectedFolder_ === this.itemId ? '0' : '-1';
  }

  /**
   * Sets the 'aria-expanded' accessibility on nodes which need it. Note that
   * aria-expanded="false" is different to having the attribute be undefined.
   */
  private updateAriaExpanded_(hasChildFolder: boolean, isOpen: boolean) {
    if (hasChildFolder) {
      this.getFocusTarget().setAttribute('aria-expanded', String(isOpen));
    } else {
      this.getFocusTarget().removeAttribute('aria-expanded');
    }
  }

  /**
   * Scrolls the folder node into view when the folder is selected.
   */
  private scrollIntoViewIfNeeded_() {
    if (!this.isSelectedFolder_) {
      return;
    }

    microTask.run(() => this.$.container.scrollIntoViewIfNeeded());
  }

  private computeIsOpen_(openState: boolean|null, depth: number): boolean {
    return openState != null ? openState :
                               depth <= FOLDER_OPEN_BY_DEFAULT_DEPTH;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'bookmarks-folder-node': BookmarksFolderNodeElement;
  }
}

customElements.define(
    BookmarksFolderNodeElement.is, BookmarksFolderNodeElement);
