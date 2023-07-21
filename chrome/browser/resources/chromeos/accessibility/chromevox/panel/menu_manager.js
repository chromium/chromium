// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class to manage the ChromeVox menus.
 */
import {AsyncUtil} from '../../common/async_util.js';
import {EventGenerator} from '../../common/event_generator.js';
import {KeyCode} from '../../common/key_code.js';
import {Command, CommandCategory, CommandStore} from '../common/command_store.js';
import {Msgs} from '../common/msgs.js';
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

  /** @param {!PanelMenu} menu */
  async addOSKeyboardShortcutsMenuItem(menu) {
    let localizedSlash =
        await AsyncUtil.getLocalizedDomKeyStringForKeyCode(KeyCode.OEM_2);
    if (!localizedSlash) {
      localizedSlash = '/';
    }
    menu.addMenuItem(
        Msgs.getMsg('open_keyboard_shortcuts_menu'),
        `Ctrl+Alt+${localizedSlash}`, '', '', async () => {
          EventGenerator.sendKeyPress(
              KeyCode.OEM_2 /* forward slash */, {'ctrl': true, 'alt': true});
        });
  }

  /**
   * Create a new search menu with the given name and add it to the menu bar.
   * @param {string} menuMsg The msg id of the new menu to add.
   * @return {!PanelMenu} The menu just created.
   */
  addSearchMenu(menuMsg) {
    this.searchMenu_ = new PanelSearchMenu(menuMsg);
    // Add event listeners to search bar.
    this.searchMenu_.searchBar.addEventListener(
        'input', event => this.onSearchBarQuery(event), false);
    this.searchMenu_.searchBar.addEventListener('mouseup', event => {
      // Clicking in the panel causes us to either activate an item or close the
      // menus altogether. Prevent that from happening if we click the search
      // bar.
      event.preventDefault();
      event.stopPropagation();
    }, false);

    $('menu-bar').appendChild(this.searchMenu_.menuBarItemElement);
    this.searchMenu_.menuBarItemElement.addEventListener(
        'mouseover',
        () =>
            this.activateMenu(this.searchMenu_, false /* activateFirstItem */),
        false);
    this.searchMenu_.menuBarItemElement.addEventListener(
        'mouseup', event => this.onMouseUpOnMenuTitle(this.searchMenu_, event),
        false);
    $('menus_background').appendChild(this.searchMenu_.menuContainerElement);
    this.menus_.push(this.searchMenu_);
    return this.searchMenu_;
  }

  /**
   * Advance the index of the current active menu by |delta|.
   * @param {number} delta The number to add to the active menu index.
   */
  advanceActiveMenuBy(delta) {
    let activeIndex = this.menus_.findIndex(menu => menu === this.activeMenu_);

    if (activeIndex >= 0) {
      activeIndex += delta;
      activeIndex = (activeIndex + this.menus_.length) % this.menus_.length;
    } else {
      if (delta >= 0) {
        activeIndex = 0;
      } else {
        activeIndex = this.menus_.length - 1;
      }
    }

    activeIndex = this.findEnabledMenuIndex(activeIndex, delta > 0 ? 1 : -1);
    if (activeIndex === -1) {
      return;
    }

    this.activateMenu(this.menus_[activeIndex], true /* activateFirstItem */);
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
   * Starting at |startIndex|, looks for an enabled menu.
   * @param {number} startIndex
   * @param {number} delta
   * @return {number} The index of the enabled menu. -1 if not found.
   */
  findEnabledMenuIndex(startIndex, delta) {
    const endIndex = (delta > 0) ? this.menus_.length : -1;
    while (startIndex !== endIndex) {
      if (this.menus_[startIndex].enabled) {
        return startIndex;
      }
      startIndex += delta;
    }
    return -1;
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
   * @param {!PanelMenu} actionsMenu
   * @param {!PanelMenu} chromevoxMenu
   * @param {!PanelMenu} jumpMenu
   * @param {!PanelMenu} speechMenu
   * @return {!Object<!CommandCategory, ?PanelMenu>}
   */
  makeCategoryMapping(actionsMenu, chromevoxMenu, jumpMenu, speechMenu) {
    return {
      [CommandCategory.ACTIONS]: actionsMenu,
      [CommandCategory.BRAILLE]: null,
      [CommandCategory.CONTROLLING_SPEECH]: speechMenu,
      [CommandCategory.DEVELOPER]: null,
      [CommandCategory.HELP_COMMANDS]: chromevoxMenu,
      [CommandCategory.INFORMATION]: speechMenu,
      [CommandCategory.JUMP_COMMANDS]: jumpMenu,
      [CommandCategory.MODIFIER_KEYS]: chromevoxMenu,
      [CommandCategory.NAVIGATION]: jumpMenu,
      [CommandCategory.NO_CATEGORY]: null,
      [CommandCategory.OVERVIEW]: jumpMenu,
      [CommandCategory.TABLES]: jumpMenu,
    };
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

  /**
   * Listens to changes in the menu search bar. Populates the search menu
   * with items that match the search bar's contents.
   * Note: we ignore PanelNodeMenu items and items without shortcuts.
   * @param {Event} event The input event.
   */
  onSearchBarQuery(event) {
    if (!this.searchMenu_) {
      throw Error('MenuManager.searchMenu_ must be defined');
    }
    const query = event.target.value.toLowerCase();
    this.searchMenu_.clear();
    // Show the search results menu.
    this.activateMenu(this.searchMenu_, false /* activateFirstItem */);
    // Populate.
    if (query) {
      for (const menu of this.menus_) {
        if (menu === this.searchMenu_ || menu instanceof PanelNodeMenu) {
          continue;
        }
        for (const item of menu.items) {
          if (!item.menuItemShortcut) {
            // Only add menu items that have shortcuts.
            continue;
          }
          const itemText = item.text.toLowerCase();
          const match = itemText.includes(query) &&
              (itemText !==
               Msgs.getMsg('panel_menu_item_none').toLowerCase()) &&
              item.enabled;
          if (match) {
            this.searchMenu_.copyAndAddMenuItem(item);
          }
        }
      }
    }

    if (this.searchMenu_.items.length === 0) {
      this.searchMenu_.addMenuItem(
          Msgs.getMsg('panel_menu_item_none'), '', '', '', function() {});
    }
    this.searchMenu_.activateItem(0);
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
