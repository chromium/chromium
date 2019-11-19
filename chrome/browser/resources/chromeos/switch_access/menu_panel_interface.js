// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interface for controllers to interact with main class of the Switch
 * Access menu panel.
 * @interface
 */
class PanelInterface {
  /**
   * Initialize the panel and buttons.
   */
  init() {}

  /**
   * Temporary function, until multiple focus rings is implemented.
   * Puts a focus ring around the given menu item.
   *
   * @param {string} id
   * @param {boolean} enable
   */
  setFocusRing(id, enable) {}

  /**
   * Sets the actions in the menu panel to the actions in |actions| from
   * the menu with the given |menuId|.
   * @param {!Array<string>} actions
   * @param {!SAConstants.MenuId} menuId
   */
  setActions(actions, menuId) {}

  /**
   * Clears the current menu from the panel.
   */
  clear() {}

  /**
   * Get the id of the current menu being shown in the panel. A null
   * id indicates that no menu is currently being shown in the panel.
   * @return {?SAConstants.MenuId}
   */
  currentMenuId() {}


  /**
   * Get the HTML element for the back button.
   * @return {Element}
   */
  backButtonElement() {}

  /**
   * Tells the menu panel to try to connect to the background page.
   */
  connectToBackground() {}
}
