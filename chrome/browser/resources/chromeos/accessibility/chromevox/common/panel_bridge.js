// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface for other renderer contexts (e.g. the
 * background context) to communicate with the ChromeVox panel.
 */

goog.provide('PanelBridge');

PanelBridge = {
  /**
   * @param {number} menuId
   * @param {!Array<!PanelNodeMenuItemData>} itemArray
   */
  async addPanelNodeMenuItems(menuId, itemArray) {
    return BridgeHelper.sendMessage(
        'Panel', 'addPanelNodeMenuItems', {menuId, itemArray});
  },
};
