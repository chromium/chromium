// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Structures to hold the data for the panel's node menus.
 */
goog.provide('PanelNodeMenuData');
goog.provide('PanelNodeMenuItemData');

PanelNodeMenuData = class {
  /**
   * @param {string} title
   * @param {number} menuId
   */
  constructor(title, menuId) {
    /** @public {string} */
    this.title = title;
    /** @public {number} */
    this.menuId = menuId;
  }
};

PanelNodeMenuItemData = class {
  /**
   * @param {string} title
   * @param {number} callbackId
   * @param {boolean} isActive True when the menu was explicitly activated and
   *     the node is focused.
   */
  constructor(title, callbackId, isActive) {
    /** @public {string} */
    this.title = title;
    /** @public {number} */
    this.callbackId = callbackId;
    /** @public {boolean} */
    this.isActive = isActive;
  }
};
