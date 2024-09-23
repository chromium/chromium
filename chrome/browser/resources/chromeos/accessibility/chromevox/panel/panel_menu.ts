// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A drop-down menu in the ChromeVox panel.
 */
import {BridgeCallbackManager} from '/common/bridge_callback_manager.js';

import {Msgs} from '../common/msgs.js';
import {PanelNodeMenuItemData} from '../common/panel_menu_data.js';

import {PanelMenuItem} from './panel_menu_item.js';

type MenuCallback = () => Promise<any>;

export class PanelMenu {
  menuBarItemElement: HTMLDivElement;
  menuContainerElement: HTMLDivElement;
  menuElement: HTMLTableElement;
  menuMsg: string;

  /** The current active menu item index, or -1 if none. */
  protected activeIndex_ = -1;
  private enabled_ = true;
  protected items_: PanelMenuItem[] = [];
  /**
   * The return value from setTimeout for a function to update the
   * scroll bars after an item has been added to a menu. Used so that we
   * don't re-layout too many times.
   */
  private updateScrollbarsTimeout_: number | null = null;

  /** @param menuMsg The msg id of the menu. */
  constructor(menuMsg: string) {
    this.menuMsg = menuMsg;
    // The item in the menu bar containing the menu's title.
    this.menuBarItemElement = document.createElement('div');
    this.menuBarItemElement.className = 'menu-bar-item';
    this.menuBarItemElement.setAttribute('role', 'menu');
    const menuTitle = Msgs.getMsg(menuMsg);
    this.menuBarItemElement.textContent = menuTitle;

    // The container for the menu. This part is fixed and scrolls its
    // contents if necessary.
    this.menuContainerElement = document.createElement('div');
    this.menuContainerElement.className = 'menu-container';
    this.menuContainerElement.style.visibility = 'hidden';

    // The menu itself. It contains all of the items, and it scrolls within
    // its container.
    this.menuElement = document.createElement('table');
    this.menuElement.className = 'menu';
    this.menuElement.setAttribute('role', 'menu');
    this.menuElement.setAttribute('aria-label', menuTitle);
    this.menuContainerElement.appendChild(this.menuElement);

    this.menuElement.addEventListener(
        'keypress', this.onKeyPress_.bind(this), true);
  }

  /**
   * @param menuItemTitle The title of the menu item.
   * @param menuItemShortcut The keystrokes to select this
   *     item.
   * @param callback The function to call if this item
   *     is selected.
   * @param id An optional id for the menu item element.
   * @return The menu item just created.
   */
  addMenuItem(
      menuItemTitle: string, menuItemShortcut: string | undefined,
      menuItemBraille: string | undefined, gesture: string | undefined,
      callback: MenuCallback, id?: string): PanelMenuItem {
    const menuItem = new PanelMenuItem(
        menuItemTitle, menuItemShortcut, menuItemBraille, gesture, callback,
        id);
    const menuElement = menuItem.element as Node;
    this.items_.push(menuItem);
    this.menuElement.appendChild(menuElement);

    // Sync the active index with focus.
    const lastItemIndex = this.items_.length - 1;
    menuElement.addEventListener(
        'focus', () => this.activeIndex_ = lastItemIndex, false);

    // Update the container height, adding a scroll bar if necessary - but
    // to avoid excessive layout, schedule this once per batch of adding
    // menu items rather than after each add.
    if (!this.updateScrollbarsTimeout_) {
      this.updateScrollbarsTimeout_ = setTimeout(() => {
        const menuBounds = this.menuElement.getBoundingClientRect();
        const maxHeight = window.innerHeight - menuBounds.top;
        this.menuContainerElement.style.maxHeight = maxHeight + 'px';
        this.updateScrollbarsTimeout_ = null;
      }, 0);
    }

    return menuItem;
  }

  /**
   * Activate this menu, which means showing it and positioning it on the
   * screen underneath its title in the menu bar.
   * @param activateFirstItem Whether or not we should activate the menu's
   *     first item.
   */
  activate(activateFirstItem: boolean): void {
    if (!this.enabled_) {
      this.menuBarItemElement.focus();
      return;
    }

    this.menuContainerElement.style.visibility = 'visible';
    this.menuContainerElement.style.opacity = String(1);
    this.menuBarItemElement.classList.add('active');
    // TODO(b/314203187): Not null asserted, check that this is correct.
    const barBounds =
        this.menuBarItemElement.parentElement!.getBoundingClientRect();
    const titleBounds = this.menuBarItemElement.getBoundingClientRect();
    const menuBounds = this.menuElement.getBoundingClientRect();

    this.menuElement.style.minWidth = titleBounds.width + 'px';
    this.menuContainerElement.style.minWidth = titleBounds.width + 'px';
    if (titleBounds.left + menuBounds.width < barBounds.width) {
      this.menuContainerElement.style.left = titleBounds.left + 'px';
    } else {
      this.menuContainerElement.style.left =
          (titleBounds.right - menuBounds.width) + 'px';
    }

    // Make the first item active.
    if (activateFirstItem) {
      this.activateItem(0);
    }
  }

  /**
   * Disables this menu. When disabled, menu contents cannot be analyzed.
   * When activated, focus gets placed on the menuBarItem (title element)
   * instead of the first menu item.
   */
  disable(): void {
    this.enabled_ = false;
    this.menuBarItemElement.classList.add('disabled');
    this.menuBarItemElement.setAttribute('aria-disabled', String(true));
    this.menuBarItemElement.setAttribute('tabindex', String(0));
    // TODO(b/314203187): Not null asserted, check that this is correct.
    this.menuBarItemElement.setAttribute(
        'aria-label', this.menuBarItemElement.textContent!);
    this.activeIndex_ = -1;
  }

  /**
   * Hide this menu. Make it invisible first to minimize spurious
   * accessibility events before the next menu activates.
   */
  deactivate(): void {
    this.menuContainerElement.style.opacity = String(0.001);
    this.menuBarItemElement.classList.remove('active');
    this.activeIndex_ = -1;

    setTimeout(() => this.menuContainerElement.style.visibility = 'hidden', 0);
  }

  /**
   * Make a specific menu item index active.
   * @param itemIndex The index of the menu item.
   */
  activateItem(itemIndex: number): void {
    this.activeIndex_ = itemIndex;
    if (this.activeIndex_ >= 0 && this.activeIndex_ < this.items_.length) {
      // TODO(b/314203187): Not null asserted, check that this is correct.
      this.items_[this.activeIndex_].element!.focus();
    }
  }

  /**
   * Advanced the active menu item index by a given number.
   * @param delta The number to add to the active menu item index.
   */
  advanceItemBy(delta: number): void {
    if (!this.enabled_) {
      return;
    }

    if (this.activeIndex_ >= 0) {
      this.activeIndex_ += delta;
      this.activeIndex_ =
          (this.activeIndex_ + this.items_.length) % this.items_.length;
    } else {
      if (delta >= 0) {
        this.activeIndex_ = 0;
      } else {
        this.activeIndex_ = this.items_.length - 1;
      }
    }

    this.activeIndex_ = this.findEnabledItemIndex_(
        this.activeIndex_, delta > 0 ? 1 : -1 /* delta */);

    if (this.activeIndex_ === -1) {
      return;
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    this.items_[this.activeIndex_].element!.focus();
  }

  /** Sets the active menu item index to be 0. */
  scrollToTop(): void {
    this.activeIndex_ = 0;
    // TODO(b/314203187): Not null asserted, check that this is correct.
    this.items_[this.activeIndex_].element!.focus();
  }

  /** Sets the active menu item index to be the last index. */
  scrollToBottom(): void {
    this.activeIndex_ = this.items_.length - 1;
    // TODO(b/314203187): Not null asserted, check that this is correct.
    this.items_[this.activeIndex_].element!.focus();
  }

  /** Get the callback for the active menu item. */
  getCallbackForCurrentItem(): MenuCallback | null {
    if (this.activeIndex_ >= 0 && this.activeIndex_ < this.items_.length) {
      return this.items_[this.activeIndex_].callback;
    }
    return null;
  }

  /** Get the callback for a menu item given its DOM element. */
  getCallbackForElement(element: HTMLElement): MenuCallback | null {
    for (let i = 0; i < this.items_.length; i++) {
      if (element === this.items_[i].element) {
        return this.items_[i].callback;
      }
    }
    return null;
  }

  /** Handles key presses for first letter accelerators. */
  private onKeyPress_(evt: KeyboardEvent): void {
    if (!this.items_.length) {
      return;
    }

    const query = String.fromCharCode(evt.charCode).toLowerCase();
    for (let i = this.activeIndex_ + 1; i !== this.activeIndex_;
         i = (i + 1) % this.items_.length) {
      if (this.items_[i].text.toLowerCase().indexOf(query) === 0) {
        this.activateItem(i);
        break;
      }
    }
  }

  get enabled(): boolean {
    return this.enabled_;
  }

  get items(): PanelMenuItem[] {
    return this.items_;
  }

  /**
   * Starting at |startIndex|, looks for an enabled menu item.
   * @return The index of the enabled item. -1 if not found.
   */
  private findEnabledItemIndex_(startIndex: number, delta: number): number {
    const endIndex = (delta > 0) ? this.items_.length : -1;
    while (startIndex !== endIndex) {
      if (this.items_[startIndex].enabled) {
        return startIndex;
      }
      startIndex += delta;
    }
    return -1;
  }
}


export class PanelNodeMenu extends PanelMenu {
  override activate(activateFirstItem: boolean): void {
    super.activate(false);
    if (activateFirstItem) {
      // The active index might have been set prior to this call in
      // |findMoreNodes|. We want to start the menu there.
      const index = this.activeIndex_ === -1 ? 0 : this.activeIndex_;
      this.activateItem(index);
    }
  }

  addItemFromData(data: PanelNodeMenuItemData): void {
    this.addMenuItem(data.title, '', '', '', async () => {
      if (data.callbackId) {
        BridgeCallbackManager.performCallback(data.callbackId);
      }
    });
    if (data.isActive) {
      this.activeIndex_ = this.items_.length - 1;
    }
  }
}

/**
 * Implements a menu that allows users to dynamically search the contents of the
 * ChromeVox menus.
 */
export class PanelSearchMenu extends PanelMenu {
  searchBar: HTMLInputElement;

  private searchResultCounter_ = 0;

  /**
   * @param menuMsg The msg id of the menu.
   */
  constructor(menuMsg: string) {
    super(menuMsg);

    // Add id attribute to the menu so we can associate it with search bar.
    this.menuElement.setAttribute('id', 'search-results');

    // Create the search bar.
    this.searchBar = document.createElement('input');
    this.searchBar.setAttribute('id', 'menus-search-bar');
    this.searchBar.setAttribute('type', 'search');
    this.searchBar.setAttribute('aria-controls', 'search-results');
    this.searchBar.setAttribute('aria-activedescendant', '');
    this.searchBar.setAttribute(
        'placeholder', Msgs.getMsg('search_chromevox_menus_placeholder'));
    this.searchBar.setAttribute(
        'aria-description', Msgs.getMsg('search_chromevox_menus_description'));
    this.searchBar.setAttribute('role', 'searchbox');

    // Create menu item to own search bar.
    const menuItem = document.createElement('tr');
    menuItem.tabIndex = -1;
    menuItem.setAttribute('role', 'menuitem');

    menuItem.appendChild(this.searchBar);

    // Add the search bar above the menu.
    this.menuContainerElement.insertBefore(menuItem, this.menuElement);
  }

  override activate(_activateFirstItem: boolean): void {
    PanelMenu.prototype.activate.call(this, false);
    if (this.searchBar.value === '') {
      this.clear();
    }
    if (this.items.length > 0) {
      this.activateItem(this.activeIndex_);
    }
    this.searchBar.focus();
  }

  override activateItem(index: number): void {
    this.resetItemAtActiveIndex();
    if (this.items.length === 0) {
      return;
    }
    if (index >= 0) {
      index = (index + this.items.length) % this.items.length;
    } else {
      if (index >= this.activeIndex_) {
        index = 0;
      } else {
        index = this.items.length - 1;
      }
    }
    this.activeIndex_ = index;
    const item = this.items[this.activeIndex_];
    // TODO(b/314203187): Not null asserted, check that this is correct.
    this.searchBar.setAttribute('aria-activedescendant', item.element!.id);
    item.element!.classList.add('active');

    // Scroll item into view, if necessary. Only check y-axis.
    const itemBounds = item.element!.getBoundingClientRect();
    const menuBarBounds = this.menuBarItemElement.getBoundingClientRect();
    const topThreshold = menuBarBounds.bottom;
    const bottomThreshold = window.innerHeight;
    if (itemBounds.bottom > bottomThreshold) {
      // Item is too far down, so align to top.
      item.element!.scrollIntoView(true /* alignToTop */);
    } else if (itemBounds.top < topThreshold) {
      // Item is too far up, so align to bottom.
      item.element!.scrollIntoView(false /* alignToTop */);
    }
  }

  override addMenuItem(
      menuItemTitle: string, menuItemShortcut: string | undefined,
      menuItemBraille: string | undefined, gesture: string | undefined,
      callback: MenuCallback, _id?: string): PanelMenuItem {
    this.searchResultCounter_ += 1;
    const item = PanelMenu.prototype.addMenuItem.call(
        this, menuItemTitle, menuItemShortcut, menuItemBraille, gesture,
        callback, 'result-number-' + this.searchResultCounter_.toString());
    // Ensure that item styling is updated on mouse hovers.
    // TODO(b/314203187): Not null asserted, check that this is correct.
    item.element!.addEventListener(
      'mouseover', () => this.resetItemAtActiveIndex(), true);
    return item;
  }

  override advanceItemBy(delta: number): void {
    this.activateItem(this.activeIndex_ + delta);
  }

  /** Clears this menu's contents. */
  clear(): void {
    this.items_ = [];
    this.activeIndex_ = -1;
    while (this.menuElement.children.length !== 0) {
      this.menuElement.removeChild(this.menuElement.firstChild as Node);
    }
    this.searchBar.setAttribute('aria-activedescendant', '');
  }

  /**
   * A convenience method to add a copy of an existing PanelMenuItem.
   * @param item The item we want to copy.
   * @return The menu item that was just created.
   */
  copyAndAddMenuItem(item: PanelMenuItem): PanelMenuItem {
    return this.addMenuItem(
        item.menuItemTitle, item.menuItemShortcut, item.menuItemBraille,
        item.gesture, item.callback);
  }

  override deactivate(): void {
    this.resetItemAtActiveIndex();
    PanelMenu.prototype.deactivate.call(this);
  }

  /** Resets the item at this.activeIndex_. */
  resetItemAtActiveIndex(): void {
    // Sanity check.
    if (this.activeIndex_ < 0 || this.activeIndex_ >= this.items.length) {
      return;
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    this.items_[this.activeIndex_].element!.classList.remove('active');
  }

  override scrollToTop(): void {
    this.activateItem(0);
  }

  override scrollToBottom(): void {
    this.activateItem(this.items_.length - 1);
  }
}
