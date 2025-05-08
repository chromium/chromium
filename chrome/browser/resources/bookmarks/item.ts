// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '/strings.m.js';

import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {selectItem} from './actions.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {BookmarksCommandManagerElement} from './command_manager.js';
import {Command, MenuSource} from './constants.js';
import {getCss} from './item.css.js';
import {getHtml} from './item.html.js';
import {StoreClientMixinLit} from './store_client_mixin_lit.js';
import type {BookmarkNode, BookmarksPageState} from './types.js';

const BookmarksItemElementBase = StoreClientMixinLit(CrLitElement);

export interface BookmarksItemElement {
  $: {
    icon: HTMLElement,
    menuButton: CrIconButtonElement,
  };
}

export class BookmarksItemElement extends BookmarksItemElementBase {
  static get is() {
    return 'bookmarks-item';
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
      ironListTabIndex: {type: Number},
      item_: {type: Object},
      isSelectedItem_: {
        type: Boolean,
        reflect: true,
      },
      isMultiSelect_: {type: Boolean},
      isFolder_: {type: Boolean},
      lastTouchPoints_: {type: Number},
      canUploadAsAccountBookmark_: {type: Boolean},
    };
  }

  accessor itemId: string = '';
  accessor ironListTabIndex: number|undefined;
  protected accessor item_: BookmarkNode|undefined;
  private accessor isSelectedItem_: boolean = false;
  private accessor isMultiSelect_: boolean = false;
  private accessor isFolder_: boolean = false;
  private accessor lastTouchPoints_: number = -1;
  // This is always false if `SyncEnableBookmarksInTransportMode` is disabled.
  protected accessor canUploadAsAccountBookmark_: boolean = false;

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.addEventListener('click', e => this.onClick_(e));
    this.addEventListener('dblclick', e => this.onDblClick_(e));
    this.addEventListener('contextmenu', e => this.onContextMenu_(e));
    this.addEventListener('keydown', e => this.onKeydown_(e));
    this.addEventListener('auxclick', e => this.onMiddleClick_(e));
    this.addEventListener('mousedown', e => this.cancelMiddleMouseBehavior_(e));
    this.addEventListener('mouseup', e => this.cancelMiddleMouseBehavior_(e));
    this.addEventListener('touchstart', e => this.onTouchStart_(e));
  }

  override connectedCallback() {
    super.connectedCallback();
    this.updateFromStore();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('itemId') && this.itemId !== '') {
      this.updateFromStore();
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('item_')) {
      this.isFolder_ = !!this.item_ && !this.item_.url;
      this.ariaLabel = this.item_?.title || this.item_?.url ||
          loadTimeData.getString('folderLabel');

      BrowserProxyImpl.getInstance()
          .getCanUploadBookmarkToAccountStorage(this.itemId)
          .then((canUpload) => {
            this.canUploadAsAccountBookmark_ = canUpload;
          });
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('item_')) {
      this.updateFavicon_(this.item_?.url);
    }
  }

  override onStateChanged(state: BookmarksPageState) {
    this.item_ = state.nodes[this.itemId];
    this.isSelectedItem_ = state.selection.items.has(this.itemId);
    this.isMultiSelect_ = state.selection.items.size > 1;
  }


  setIsSelectedItemForTesting(selected: boolean) {
    this.isSelectedItem_ = selected;
  }

  focusMenuButton() {
    focusWithoutInk(this.$.menuButton);
  }

  getDropTarget(): BookmarksItemElement {
    return this;
  }

  private onContextMenu_(e: MouseEvent) {
    e.preventDefault();
    e.stopPropagation();

    // Prevent context menu from appearing after a drag, but allow opening the
    // context menu through 2 taps
    const capabilities = (e as unknown as {
                           sourceCapabilities: {firesTouchEvents?: boolean},
                         }).sourceCapabilities;
    if (capabilities && capabilities.firesTouchEvents &&
        this.lastTouchPoints_ !== 2) {
      return;
    }

    this.focus();
    if (!this.isSelectedItem_) {
      this.selectThisItem_();
    }

    this.dispatchEvent(new CustomEvent('open-command-menu', {
      bubbles: true,
      composed: true,
      detail: {
        x: e.clientX,
        y: e.clientY,
        source: MenuSource.ITEM,
        targetId: this.itemId,
      },
    }));
  }

  protected onMenuButtonClick_(e: Event) {
    e.stopPropagation();
    e.preventDefault();

    // Skip selecting the item if this item is part of a multi-selected group.
    if (!this.isMultiSelectMenu_()) {
      this.selectThisItem_();
    }

    this.dispatchEvent(new CustomEvent('open-command-menu', {
      bubbles: true,
      composed: true,
      detail: {
        targetElement: e.target,
        source: MenuSource.ITEM,
        targetId: this.itemId,
      },
    }));
  }

  protected onUploadButtonClick_() {
    // Skip selecting the item if this item is part of a multi-selected group.
    if (!this.isMultiSelectMenu_()) {
      this.selectThisItem_();
    }

    BrowserProxyImpl.getInstance().onSingleBookmarkUploadClicked(this.itemId);
  }

  private selectThisItem_() {
    this.dispatch(selectItem(this.itemId, this.getState(), {
      clear: true,
      range: false,
      toggle: false,
    }));
  }

  protected getItemUrl_(): string {
    return this.item_?.url || '';
  }

  protected getItemTitle_(): string {
    return this.item_?.title || '';
  }

  private onClick_(e: MouseEvent) {
    // Ignore double clicks so that Ctrl double-clicking an item won't deselect
    // the item before opening.
    if (e.detail !== 2) {
      const addKey = isMac ? e.metaKey : e.ctrlKey;
      this.dispatch(selectItem(this.itemId, this.getState(), {
        clear: !addKey,
        range: e.shiftKey,
        toggle: addKey && !e.shiftKey,
      }));
    }
    e.stopPropagation();
    e.preventDefault();
  }

  private onKeydown_(e: KeyboardEvent) {
    const cursorModifier = isMac ? e.metaKey : e.ctrlKey;
    if (e.key === 'ArrowLeft') {
      this.focus();
    } else if (e.key === 'ArrowRight') {
      this.$.menuButton.focus();
    } else if (e.key === ' ' && !cursorModifier) {
      // Spacebar with the modifier is handled by the list.
      this.dispatch(selectItem(this.itemId, this.getState(), {
        clear: false,
        range: false,
        toggle: true,
      }));
    }
  }

  private onDblClick_(_e: MouseEvent) {
    if (!this.isSelectedItem_) {
      this.selectThisItem_();
    }

    const commandManager = BookmarksCommandManagerElement.getInstance();
    const itemSet = this.getState().selection.items;
    if (commandManager.canExecute(Command.OPEN, itemSet)) {
      commandManager.handle(Command.OPEN, itemSet);
    }
  }

  private onMiddleClick_(e: MouseEvent) {
    if (e.button !== 1) {
      return;
    }

    this.selectThisItem_();
    if (this.isFolder_) {
      return;
    }

    const commandManager = BookmarksCommandManagerElement.getInstance();
    const itemSet = this.getState().selection.items;
    const command = e.shiftKey ? Command.OPEN : Command.OPEN_NEW_TAB;
    if (commandManager.canExecute(command, itemSet)) {
      commandManager.handle(command, itemSet);
    }
  }

  private onTouchStart_(e: TouchEvent) {
    this.lastTouchPoints_ = e.touches.length;
  }

  /**
   * Prevent default middle-mouse behavior. On Windows, this prevents autoscroll
   * (during mousedown), and on Linux this prevents paste (during mouseup).
   */
  private cancelMiddleMouseBehavior_(e: MouseEvent) {
    if (e.button === 1) {
      e.preventDefault();
    }
  }

  private updateFavicon_(url?: string) {
    this.$.icon.className =
        url ? 'website-icon' : 'folder-icon icon-folder-open';
    this.$.icon.style.backgroundImage =
        url ? getFaviconForPageURL(url, false) : '';
  }

  protected getButtonAriaLabel_(): string {
    if (!this.item_) {
      return '';  // Item hasn't loaded, skip for now.
    }

    if (this.isMultiSelectMenu_()) {
      return loadTimeData.getStringF('moreActionsMultiButtonAxLabel');
    }

    return loadTimeData.getStringF(
        'moreActionsButtonAxLabel', this.item_.title);
  }

  /**
   * This item is part of a group selection.
   */
  private isMultiSelectMenu_(): boolean {
    return this.isSelectedItem_ && this.isMultiSelect_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'bookmarks-item': BookmarksItemElement;
  }
}

customElements.define(BookmarksItemElement.is, BookmarksItemElement);
