// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class to manage the ChromeVox menus.
 */
import {Command, CommandStore} from '../common/command_store.js';

import {PanelMenu, PanelSearchMenu} from './panel_menu.js';

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

    /** @private {?PanelSearchMenu} */
    this.searchMenu_ = null;
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

  /** @return {?PanelSearchMenu} */
  get searchMenu() {
    return this.searchMenu_;
  }
  /** @param {?PanelSearchMenu} menu */
  set searchMenu(menu) {
    this.searchMenu_ = menu;
  }
}
