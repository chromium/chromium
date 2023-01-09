// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class to manage the ChromeVox menus.
 */
import {Command, CommandStore} from '../common/command_store.js';

import {PanelMenu, PanelSearchMenu} from './panel_menu.js';

export class MenuManager {
  constructor() {
    /**
     * The array of top-level menus.
     * @private {!Array<PanelMenu>}
     */
    this.menus_ = [];

    /** @private {?PanelSearchMenu} */
    this.searchMenu_ = null;
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
   * Temporary method during migration from panel.js.
   * @return {!Array<!PanelMenu>}
   */
  get menus() {
    return this.menus_;
  }

  /**
   * Temporary method during migration from panel.js.
   * @return {?PanelSearchMenu}
   */
  get searchMenu() {
    return this.searchMenu_;
  }
  /**
   * Temporary method during migration from panel.js.
   * @param {?PanelSearchMenu} menu
   */
  set searchMenu(menu) {
    this.searchMenu_ = menu;
  }
}
