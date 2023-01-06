// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A drop-down menu in the ChromeVox panel.
 */
import {BackgroundBridge} from '../common/background_bridge.js';
import {BridgeCallbackManager} from '../common/bridge_callback_manager.js';
import {Msgs} from '../common/msgs.js';
import {PanelNodeMenuItemData} from '../common/panel_menu_data.js';

import {PanelMenuItem} from './panel_menu_item.js';

export class PanelMenu {
  /**
   * @param {string} menuMsg The msg id of the menu.
   */
  constructor(menuMsg) {
    /** @type {string} */
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

    /**
     * The items in the menu.
     * @type {!Array<!PanelMenuItem>}
     * @private
     */
    this.items_ = [];

    /**
     * The return value from setTimeout for a function to update the
     * scroll bars after an item has been added to a menu. Used so that we
     * don't re-layout too many times.
     * @type {?number}
     * @private
     */
    this.updateScrollbarsTimeout_ = null;

    /**
     * The current active menu item index, or -1 if none.
     * @type {number}
     * @private
     */
    this.activeIndex_ = -1;

    this.menuElement.addEventListener(
        'keypress', this.onKeyPress_.bind(this), true);

    /** @private {boolean} */
    this.enabled_ = true;
  }

  /**
   * @param {string} menuItemTitle The title of the menu item.
   * @param {string} menuItemShortcut The keystrokes to select this item.
   * @param {string} menuItemBraille
   * @param {string} gesture
   * @param {function() : !Promise} callback The function to call if this item
   *     is selected.
   * @param {string=} opt_id An optional id for the menu item element.
   * @return {!PanelMenuItem} The menu item just created.
   */
  addMenuItem(
      menuItemTitle, menuItemShortcut, menuItemBraille, gesture, callback,
      opt_id) {
    const menuItem = new PanelMenuItem(
        menuItemTitle, menuItemShortcut, menuItemBraille, gesture, callback,
        opt_id);
    this.items_.push(menuItem);
    this.menuElement.appendChild(menuItem.element);

    // Sync the active index with focus.
    menuItem.element.addEventListener(
        'focus', (function(index, event) {
                   this.activeIndex_ = index;
                 }).bind(this, this.items_.length - 1),
        false);

    // Update the container height, adding a scroll bar if necessary - but
    // to avoid excessive layout, schedule this once per batch of adding
    // menu items rather than after each add.
    if (!this.updateScrollbarsTimeout_) {
      this.updateScrollbarsTimeout_ = setTimeout(
          (function() {
            const menuBounds = this.menuElement.getBoundingClientRect();
            const maxHeight = window.innerHeight - menuBounds.top;
            this.menuContainerElement.style.maxHeight = maxHeight + 'px';
            this.updateScrollbarsTimeout_ = null;
          }).bind(this),
          0);
    }

    return menuItem;
  }

  /**
   * Activate this menu, which means showing it and positioning it on the
   * screen underneath its title in the menu bar.
   * @param {boolean} activateFirstItem Whether or not we should activate the
   *     menu's
   * first item.
   */
  activate(activateFirstItem) {
    if (!this.enabled_) {
      this.menuBarItemElement.focus();
      return;
    }

    this.menuContainerElement.style.visibility = 'visible';
    this.menuContainerElement.style.opacity = 1;
    this.menuBarItemElement.classList.add('active');
    const barBounds =
        this.menuBarItemElement.parentElement.getBoundingClientRect();
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
  disable() {
    this.enabled_ = false;
    this.menuBarItemElement.classList.add('disabled');
    this.menuBarItemElement.setAttribute('aria-disabled', true);
    this.menuBarItemElement.setAttribute('tabindex', 0);
    this.menuBarItemElement.setAttribute(
        'aria-label', this.menuBarItemElement.textContent);
    this.activeIndex_ = -1;
  }

  /**
   * Hide this menu. Make it invisible first to minimize spurious
   * accessibility events before the next menu activates.
   */
  deactivate() {
    this.menuContainerElement.style.opacity = 0.001;
    this.menuBarItemElement.classList.remove('active');
    this.activeIndex_ = -1;

    setTimeout(
        (function() {
          this.menuContainerElement.style.visibility = 'hidden';
        }).bind(this),
        0);
  }

  /**
   * Make a specific menu item index active.
   * @param {number} itemIndex The index of the menu item.
   */
  activateItem(itemIndex) {
    this.activeIndex_ = itemIndex;
    if (this.activeIndex_ >= 0 && this.activeIndex_ < this.items_.length) {
      this.items_[this.activeIndex_].element.focus();
    }
  }

  /**
   * Advanced the active menu item index by a given number.
   * @param {number} delta The number to add to the active menu item index.
   */
  advanceItemBy(delta) {
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

    this.items_[this.activeIndex_].element.focus();
  }

  /**
   * Sets the active menu item index to be 0.
   */
  scrollToTop() {
    this.activeIndex_ = 0;
    this.items_[this.activeIndex_].element.focus();
  }

  /**
   * Sets the active menu item index to be the last index.
   */
  scrollToBottom() {
    this.activeIndex_ = this.items_.length - 1;
    this.items_[this.activeIndex_].element.focus();
  }

  /**
   * Get the callback for the active menu item.
   * @return {?function() : !Promise} The callback.
   */
  getCallbackForCurrentItem() {
    if (this.activeIndex_ >= 0 && this.activeIndex_ < this.items_.length) {
      return this.items_[this.activeIndex_].callback;
    }
    return null;
  }

  /**
   * Get the callback for a menu item given its DOM element.
   * @param {Element} element The DOM element.
   * @return {?function() : !Promise} The callback.
   */
  getCallbackForElement(element) {
    for (let i = 0; i < this.items_.length; i++) {
      if (element === this.items_[i].element) {
        return this.items_[i].callback;
      }
    }
    return null;
  }

  /**
   * Handles key presses for first letter accelerators.
   */
  onKeyPress_(evt) {
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

  /**
   * @return {boolean} The enabled state of this menu.
   */
  get enabled() {
    return this.enabled_;
  }

  /**
   * @return {!Array<!PanelMenuItem>}
   */
  get items() {
    return this.items_;
  }

  /**
   * Starting at |startIndex|, looks for an enabled menu item.
   * @param {number} startIndex
   * @param {number} delta
   * @return {number} The index of the enabled item. -1 if not found.
   * @private
   */
  findEnabledItemIndex_(startIndex, delta) {
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
  /** @override */
  activate(activateFirstItem) {
    super.activate(false);
    if (activateFirstItem) {
      // The active index might have been set prior to this call in
      // |findMoreNodes|. We want to start the menu there.
      const index = this.activeIndex_ === -1 ? 0 : this.activeIndex_;
      this.activateItem(index);
    }
  }

  /** @param {!PanelNodeMenuItemData} data */
  addItemFromData(data) {
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
  /**
   * @param {!string} menuMsg The msg id of the menu.
   */
  constructor(menuMsg) {
    super(menuMsg);
    this.searchResultCounter_ = 0;

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

  /** @override */
  activate(activateFirstItem) {
    PanelMenu.prototype.activate.call(this, false);
    if (this.searchBar.value === '') {
      this.clear();
    }
    if (this.items_.length > 0) {
      this.activateItem(this.activeIndex_);
    }
    this.searchBar.focus();
  }

  /** @override */
  activateItem(index) {
    this.resetItemAtActiveIndex();
    if (this.items_.length === 0) {
      return;
    }
    if (index >= 0) {
      index = (index + this.items_.length) % this.items_.length;
    } else {
      if (index >= this.activeIndex_) {
        index = 0;
      } else {
        index = this.items_.length - 1;
      }
    }
    this.activeIndex_ = index;
    const item = this.items_[this.activeIndex_];
    this.searchBar.setAttribute('aria-activedescendant', item.element.id);
    item.element.classList.add('active');

    // Scroll item into view, if necessary. Only check y-axis.
    const itemBounds = item.element.getBoundingClientRect();
    const menuBarBounds = this.menuBarItemElement.getBoundingClientRect();
    const topThreshold = menuBarBounds.bottom;
    const bottomThreshold = window.innerHeight;
    if (itemBounds.bottom > bottomThreshold) {
      // Item is too far down, so align to top.
      item.element.scrollIntoView(true /* alignToTop */);
    } else if (itemBounds.top < topThreshold) {
      // Item is too far up, so align to bottom.
      item.element.scrollIntoView(false /* alignToTop */);
    }
  }

  /** @override */
  addMenuItem(
      menuItemTitle, menuItemShortcut, menuItemBraille, gesture, callback,
      opt_id) {
    this.searchResultCounter_ += 1;
    const item = PanelMenu.prototype.addMenuItem.call(
        this, menuItemTitle, menuItemShortcut, menuItemBraille, gesture,
        callback, 'result-number-' + this.searchResultCounter_.toString());
    // Ensure that item styling is updated on mouse hovers.
    item.element.addEventListener('mouseover', event => {
      this.resetItemAtActiveIndex();
    }, true);
    return item;
  }

  /** @override */
  advanceItemBy(delta) {
    this.activateItem(this.activeIndex_ + delta);
  }

  /**
   * Clears this menu's contents.
   */
  clear() {
    this.items_ = [];
    this.activeIndex_ = -1;
    while (this.menuElement.children.length !== 0) {
      this.menuElement.removeChild(this.menuElement.firstChild);
    }
    this.searchBar.setAttribute('aria-activedescendant', '');
  }

  /**
   * A convenience method to add a copy of an existing PanelMenuItem.
   * @param {!PanelMenuItem} item The item we want to copy.
   * @return {!PanelMenuItem} The menu item that was just created.
   */
  copyAndAddMenuItem(item) {
    return this.addMenuItem(
        item.menuItemTitle, item.menuItemShortcut, item.menuItemBraille,
        item.gesture, item.callback);
  }

  /** @override */
  deactivate() {
    this.resetItemAtActiveIndex();
    PanelMenu.prototype.deactivate.call(this);
  }

  /**
   * Resets the item at this.activeIndex_.
   */
  resetItemAtActiveIndex() {
    // Sanity check.
    if (this.activeIndex_ < 0 || this.activeIndex_ >= this.items.length) {
      return;
    }

    this.items_[this.activeIndex_].element.classList.remove('active');
  }

  /** @override */
  scrollToTop() {
    this.activateItem(0);
  }

  /** @override */
  scrollToBottom() {
    this.activateItem(this.items_.length - 1);
  }
}
