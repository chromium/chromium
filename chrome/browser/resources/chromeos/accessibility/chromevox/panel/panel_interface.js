// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An interface to control the ChromeVox Panel.
 */

export class PanelInterface {
  /**
   * Close the menus and restore focus to the page. If a menu item's callback
   * was queued, execute it once focus is restored.
   */
  closeMenusAndRestoreFocus() {}

  /**
   * A callback function to be executed to perform the action from selecting
   * a menu item after the menu has been closed and focus has been restored
   * to the page or wherever it was previously.
   * @param {?Function} callback
   */
  setPendingCallback(callback) {}
}

/** @type {PanelInterface} */
PanelInterface.instance;
