// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface for other renderers to communicate with
 * the ChromeVox panel.
 */
import {BridgeHelper} from '/common/bridge_helper.js';

import {BridgeConstants} from './bridge_constants.js';
import {PanelNodeMenuItemData} from './panel_menu_data.js';

export class PanelBridge {
  static addMenuItem(itemData: PanelNodeMenuItemData): Promise<void> {
    return BridgeHelper.sendMessage(
        BridgeConstants.Panel.TARGET,
        BridgeConstants.Panel.Action.ADD_MENU_ITEM, itemData);
  }

  static async onCurrentRangeChanged(): Promise<void> {
    return BridgeHelper.sendMessage(
        BridgeConstants.Panel.TARGET,
        BridgeConstants.Panel.Action.ON_CURRENT_RANGE_CHANGED);
  }
}
