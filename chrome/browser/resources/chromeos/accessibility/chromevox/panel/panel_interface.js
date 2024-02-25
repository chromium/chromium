// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An interface to control the ChromeVox Panel.
 */
import {PanelMode} from './panel_mode.js';

export class PanelInterface {
  /**
   * Close the menus and restore focus to the page. If a menu item's callback
   * was queued, execute it once focus is restored.
   */
  async closeMenusAndRestoreFocus() {}

  /** @return {!PanelMode} */
  get mode() {}

  /** @param {!PanelMode} mode */
  setMode(mode) {}

  /** @return {string} */
  get sessionState() {}

  /** Exit ChromeVox. */
  onClose() {}

  /**
   * A callback function to be executed to perform the action from selecting
   * a menu item after the menu has been closed and focus has been restored
   * to the page or wherever it was previously.
   * @param {?function() : !Promise} callback
   */
  setPendingCallback(callback) {}
}

/** @type {PanelInterface} */
PanelInterface.instance;
