// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface for other renderers to communicate with
 * the ChromeVox panel.
 */
import {BridgeHelper} from '/common/bridge_helper.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BridgeConstants} from './bridge_constants.js';
import {PanelCommand} from './panel_command.js';
import type {PanelNodeMenuItemData} from './panel_menu_data.js';
import {MenuDataForTest} from './panel_menu_data.js';

const PanelTarget = BridgeConstants.Panel.TARGET;
const PanelAction = BridgeConstants.Panel.Action;

const PanelTestTarget = BridgeConstants.PanelTest.TARGET;
const PanelTestAction = BridgeConstants.PanelTest.Action;

export class PanelBridge {
  static addMenuItem(itemData: PanelNodeMenuItemData): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTarget, PanelAction.ADD_MENU_ITEM, itemData);
  }

  static async onCurrentRangeChanged(): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTarget, PanelAction.ON_CURRENT_RANGE_CHANGED);
  }

  static execCommand(panelCommand: PanelCommand): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTarget, PanelAction.EXEC_COMMAND,
        {type: panelCommand.type, data: panelCommand.data});
  }


  static braillePanLeftForTest(): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.BRAILLE_PAN_LEFT);
  }

  static braillePanRightForTest(): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.BRAILLE_PAN_RIGHT)
  }

  static disableMessagesForTest(): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.DISABLE_ERROR_MSG);
  }

  static fireMockEventForTest(key: string): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.FIRE_MOCK_EVENT, key);
  }

  static fireMockQueryForTest(query: string): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.FIRE_MOCK_QUERY, query);
  }

  static getActiveMenuDataForTest(): Promise<MenuDataForTest> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.GET_ACTIVE_MENU_DATA);
  }

  static getActiveSearchMenuDataForTest(): Promise<MenuDataForTest> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.GET_ACTIVE_SEARCH_MENU_DATA);
  }
}

TestImportManager.exportForTesting(PanelBridge);
