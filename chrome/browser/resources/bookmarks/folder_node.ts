// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_ripple/cr_ripple.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {isRTL} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {changeFolderOpen, selectFolder} from './actions.js';
import {BookmarksCommandManagerElement} from './command_manager.js';
import {LOCAL_HEADING_NODE_ID, MenuSource, ROOT_NODE_ID} from './constants.js';
import {getCss} from './folder_node.css.js';
import {getHtml} from './folder_node.html.js';
import {StoreClientMixinLit} from './store_client_mixin_lit.js';
import type {BookmarkNode, BookmarksPageState} from './types.js';
import {hasChildFolders, isRootNode, isShowingSearch} from './util.js';

const BookmarksFolderNodeElementBase = StoreClientMixinLit(CrLitElement);

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

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      itemId: {type: String},
      depth: {type: Number},
      isOpen: {type: Boolean},
      item_: {type: Object},
      openState_: {type: Boolean},
      selectedFolder_: {type: String},
      searchActive_: {type: Boolean},

      isSelectedFolder_: {
        type: Boolean,
        reflect: true,
      },

      hasChildFolder_: {type: Boolean},
    };
  }

  accessor depth: number = -1;
  accessor isOpen: boolean = false;
  accessor itemId: string = '';
  protected accessor item_: BookmarkNode|undefined;
  private accessor openState_: boolean|null = null;
  private accessor selectedFolder_: string = '';
  private accessor searchActive_: boolean = false;
  protected accessor isSelectedFolder_: boolean = false;
  protected accessor hasChildFolder_: boolean = false;

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.addEventListener('keydown', e => this.onKeydown_(e));
  }

  override connectedCallback() {
    super.connectedCallback();
    this.updateFromStore();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('itemId')) {
      this.updateFromStore();
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedProperties.has('depth') ||
        changedPrivateProperties.has('openState_')) {
      // If account nodes exist, the permanent account nodes should be visible,
      // while the local ones are collapsed.
      const defaultOpenState =
          isRootNode(this.itemId) && this.itemId !== LOCAL_HEADING_NODE_ID;
      this.isOpen =
          this.openState_ !== null ? this.openState_ : defaultOpenState;
    }

    if (changedProperties.has('itemId') ||
        changedPrivateProperties.has('selectedFolder_') ||
        changedPrivateProperties.has('searchActive_')) {
      const previous = this.isSelectedFolder_;
      this.isSelectedFolder_ =
          this.itemId === this.selectedFolder_ && !this.searchActive_;
      if (previous !== this.isSelectedFolder_ && this.isSelectedFolder_) {
        this.scrollIntoViewIfNeeded_();
      }
    }

    if (changedPrivateProperties.has('item_')) {
      this.hasChildFolder_ =
          hasChildFolders(this.itemId, this.getState().nodes);
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('depth')) {
      this.style.setProperty('--node-depth', String(this.depth));
      if (this.depth === -1) {
        this.$.descendants.removeAttribute('role');
      }
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedProperties.has('isOpen') ||
        changedPrivateProperties.has('hasChildFolder_')) {
      this.updateAriaExpanded_();
    }
  }

  // StoreClientMixinLit
  override onStateChanged(state: BookmarksPageState) {
    this.item_ = state.nodes[this.itemId];
    this.openState_ = state.folderOpenState.has(this.itemId) ?
        state.folderOpenState.get(this.itemId)! :
        null;
    this.selectedFolder_ = state.selectedFolder;
    this.searchActive_ = isShowingSearch(state);
  }

  protected getContainerClass_(): string {
    return this.isSelectedFolder_ ? 'selected' : '';
  }

  protected getItemTitle_(): string {
    return this.item_?.title || '';
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
        xDirection, yDirection, this.shadowRoot.activeElement);

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
        (currentFocus as HTMLElement).tagName === 'BOOKMARKS-FOLDER-NODE';

    if (xDirection === 1) {
      // The right arrow opens a folder if closed and goes to the first child
      // otherwise.
      if (this.hasChildFolder_) {
        if (!this.isOpen) {
          this.dispatch(changeFolderOpen(this.item_!.id, true));
        } else {
          yDirection = 1;
        }
      }
    } else if (xDirection === -1) {
      // The left arrow closes a folder if open and goes to the parent
      // otherwise.
      if (this.hasChildFolder_ && this.isOpen) {
        this.dispatch(changeFolderOpen(this.item_!.id, false));
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
            yDirection === -1, (currentFocus as BookmarksFolderNodeElement));
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

  protected selectFolder_() {
    if (!this.isSelectedFolder_) {
      this.dispatch(selectFolder(this.itemId, this.getState().nodes));
    }
  }

  protected onContextMenu_(e: MouseEvent) {
    e.preventDefault();
    this.selectFolder_();
    // Disable the context menu for root nodes.
    if (isRootNode(this.itemId)) {
      return;
    }
    BookmarksCommandManagerElement.getInstance().openCommandMenuAtPosition(
        e.clientX, e.clientY, MenuSource.TREE, new Set([this.itemId]));
  }

  private getChildFolderNodes_(): BookmarksFolderNodeElement[] {
    return Array.from(
        this.shadowRoot.querySelectorAll('bookmarks-folder-node'));
  }

  /**
   * Toggles whether the folder is open.
   */
  protected toggleFolder_(e: Event) {
    this.dispatch(changeFolderOpen(this.itemId, !this.isOpen));
    e.stopPropagation();
  }

  protected preventDefault_(e: Event) {
    e.preventDefault();
  }

  protected getChildDepth_(): number {
    return this.depth + 1;
  }

  protected getFolderChildren_(): string[] {
    const children = this.item_?.children;
    const nodes = this.getState()?.nodes;
    if (!Array.isArray(children) || !nodes) {
      return [];
    }
    return children.filter(itemId => {
      return !nodes[itemId]?.url;  // safely access .url only if node exists
    });
  }

  protected isRootFolder_(): boolean {
    return this.itemId === ROOT_NODE_ID;
  }

  protected getTabIndex_(): string {
    // This returns a tab index of 0 for the cached selected folder when the
    // search is active, even though this node is not technically selected. This
    // allows the sidebar to be focusable during a search.
    return this.selectedFolder_ === this.itemId ? '0' : '-1';
  }

  protected getAriaLevel_(): number {
    // Converts (-1)-indexed depth to 1-based ARIA level.
    return this.depth + 2;
  }

  /**
   * Sets the 'aria-expanded' accessibility on nodes which need it. Note that
   * aria-expanded="false" is different to having the attribute be undefined.
   */
  private updateAriaExpanded_() {
    if (this.hasChildFolder_) {
      this.getFocusTarget().setAttribute('aria-expanded', String(this.isOpen));
    } else {
      this.getFocusTarget().removeAttribute('aria-expanded');
    }
  }

  /**
   * Scrolls the folder node into view when the folder is selected.
   */
  private async scrollIntoViewIfNeeded_() {
    await this.updateComplete;
    this.$.container.scrollIntoViewIfNeeded();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'bookmarks-folder-node': BookmarksFolderNodeElement;
  }
}

customElements.define(
    BookmarksFolderNodeElement.is, BookmarksFolderNodeElement);
