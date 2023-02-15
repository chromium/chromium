// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class to manage the ChromeVox menus.
 */
import {Command, CommandStore} from '../common/command_store.js';
import {PanelNodeMenuData, PanelNodeMenuId, PanelNodeMenuItemData} from '../common/panel_menu_data.js';

import {PanelInterface} from './panel_interface.js';
import {PanelMenu, PanelNodeMenu, PanelSearchMenu} from './panel_menu.js';

const $ = (id) => document.getElementById(id);

export class MenuManager {
  constructor() {
    /**
     * The currently active menu, if any.
     * @private {?PanelMenu}
     */
    this.activeMenu_ = null;

    /** @private {string} */
    this.lastMenu_ = '';

    /**
     * The array of top-level menus.
     * @private {!Array<!PanelMenu>}
     */
    this.menus_ = [];

    /** @private {!Object<!PanelNodeMenuId, !PanelNodeMenu>} */
    this.nodeMenuDictionary_ = {};

    /** @private {?PanelSearchMenu} */
    this.searchMenu_ = null;
  }

  /**
   * Activate a menu, which implies hiding the previous active menu.
   * @param {?PanelMenu} menu The new menu to activate.
   * @param {boolean} activateFirstItem Whether or not we should activate the
   *     menu's first item.
   */
  activateMenu(menu, activateFirstItem) {
    if (menu === this.activeMenu_) {
      return;
    }

    if (this.activeMenu_) {
      this.activeMenu_.deactivate();
      this.activeMenu_ = null;
    }

    this.activeMenu_ = menu;
    PanelInterface.instance.setPendingCallback(null);

    if (this.activeMenu_) {
      this.activeMenu_.activate(activateFirstItem);
    }
  }

  /**
   * Create a new menu with the given name and add it to the menu bar.
   * @param {string} menuMsg The msg id of the new menu to add.
   * @return {!PanelMenu} The menu just created.
   */
  addMenu(menuMsg) {
    const menu = new PanelMenu(menuMsg);
    $('menu-bar').appendChild(menu.menuBarItemElement);
    menu.menuBarItemElement.addEventListener(
        'mouseover',
        () => this.activateMenu(menu, true /* activateFirstItem */), false);
    menu.menuBarItemElement.addEventListener(
        'mouseup', event => this.onMouseUpOnMenuTitle(menu, event), false);
    $('menus_background').appendChild(menu.menuContainerElement);
    this.menus_.push(menu);
    return menu;
  }

  /**
   * Create a new node menu with the given name and add it to the menu bar.
   * @param {!PanelNodeMenuData} menuData The title/predicate for the new menu.
   */
  addNodeMenu(menuData) {
    const menu = new PanelNodeMenu(menuData.titleId);
    $('menu-bar').appendChild(menu.menuBarItemElement);
    menu.menuBarItemElement.addEventListener(
        'mouseover',
        () => this.activateMenu(menu, true /* activateFirstItem */));
    menu.menuBarItemElement.addEventListener(
        'mouseup', event => this.onMouseUpOnMenuTitle(menu, event));
    $('menus_background').appendChild(menu.menuContainerElement);
    this.menus_.push(menu);
    this.nodeMenuDictionary_[menuData.menuId] = menu;
  }

  /** @param {!PanelNodeMenuItemData} itemData */
  addNodeMenuItem(itemData) {
    this.nodeMenuDictionary_[itemData.menuId].addItemFromData(itemData);
  }

  /**
   * Clear any previous menus. The menus are all regenerated each time the
   * menus are opened.
   */
  clearMenus() {
    while (this.menus_.length) {
      const menu = this.menus_.pop();
      $('menu-bar').removeChild(menu.menuBarItemElement);
      $('menus_background').removeChild(menu.menuContainerElement);
    }
    if (this.activeMenu_) {
      this.lastMenu_ = this.activeMenu_.menuMsg;
    }
    this.activeMenu_ = null;
  }

  /** Disables menu items that are prohibited without a signed-in user. */
  denySignedOut() {
    for (const menu of this.menus_) {
      for (const item of menu.items) {
        if (CommandStore.denySignedOut(
                /** @type {!Command} */ (item.element.id))) {
          item.disable();
        }
      }
    }
  }

  /**
   * @param {string|undefined} opt_menuTitle
   * @return {!PanelMenu}
   */
  getSelectedMenu(opt_menuTitle) {
    const specifiedMenu =
        this.menus_.find(menu => menu.menuMsg === opt_menuTitle);
    return specifiedMenu || this.searchMenu_ || this.menus_[0];
  }

  /**
   * Activate a menu whose title has been clicked. Stop event propagation at
   * this point so we don't close the ChromeVox menus and restore focus.
   * @param {PanelMenu} menu The menu we would like to activate.
   * @param {Event} mouseUpEvent The mouseup event.
   */
  onMouseUpOnMenuTitle(menu, mouseUpEvent) {
    this.activateMenu(menu, true /* activateFirstItem */);
    mouseUpEvent.preventDefault();
    mouseUpEvent.stopPropagation();
  }

  // The following getters and setters are temporary during the migration from
  // panel.js.

  /** @return {?PanelMenu} */
  get activeMenu() {
    return this.activeMenu_;
  }
  /** @param {?PanelMenu} menu */
  set activeMenu(menu) {
    this.activeMenu_ = menu;
  }

  /** @return {string} */
  get lastMenu() {
    return this.lastMenu_;
  }
  /** @param {string} menuMsg */
  set lastMenu(menuMsg) {
    this.lastMenu_ = menuMsg;
  }

  /** @return {!Array<!PanelMenu>} */
  get menus() {
    return this.menus_;
  }

  /** @return {!Object<!PanelNodeMenuId, !PanelNodeMenu>} */
  get nodeMenuDictionary() {
    return this.nodeMenuDictionary_;
  }

  /** @return {?PanelSearchMenu} */
  get searchMenu() {
    return this.searchMenu_;
  }
  /** @param {?PanelSearchMenu} menu */
  set searchMenu(menu) {
    this.searchMenu_ = menu;
  }
}
