// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface for other renderers to communicate with
 * the ChromeVox panel.
 */

goog.provide('PanelBridge');

goog.require('PanelNodeMenuItemData');

PanelBridge = {
  /**
   * @param {!PanelNodeMenuItemData} itemData
   * @return {!Promise}
   */
  addMenuItem(itemData) {
    return BridgeHelper.sendMessage(
        BridgeTarget.PANEL, BridgeAction.ADD_MENU_ITEM, itemData);
  },

  /** @return {!Promise} */
  async onCurrentRangeChanged() {
    return BridgeHelper.sendMessage(
        BridgeTarget.PANEL, BridgeAction.ON_CURRENT_RANGE_CHANGED);
  },
};
