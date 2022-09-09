// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface for other renderers to communicate with
 * the ChromeVox panel.
 */

import {BridgeConstants} from './bridge_constants.js';
import {BridgeHelper} from './bridge_helper.js';
import {PanelNodeMenuItemData} from './panel_menu_data.js';

export class PanelBridge {
  /**
   * @param {!PanelNodeMenuItemData} itemData
   * @return {!Promise}
   */
  static addMenuItem(itemData) {
    return BridgeHelper.sendMessage(
        BridgeConstants.Panel.TARGET,
        BridgeConstants.Panel.Action.ADD_MENU_ITEM, itemData);
  }

  /** @return {!Promise} */
  static async onCurrentRangeChanged() {
    return BridgeHelper.sendMessage(
        BridgeConstants.Panel.TARGET,
        BridgeConstants.Panel.Action.ON_CURRENT_RANGE_CHANGED);
  }
}
