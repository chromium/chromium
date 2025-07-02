// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['panel_test_base.js']);

/** Test fixture for MenuManager. */
ChromeVoxMenuManagerTest = class extends ChromeVoxPanelTestBase {
  getMenuManager() {
    return this.getPanel().instance.getMenuManagerForTesting();
  }

  async runTestInPanelManager(testToPerform) {
    const TARGET = BridgeConstants.PanelTest.TARGET;
    await BridgeHelper.sendMessage(
        TARGET, BridgeConstants.PanelTest.Action.REPLACE_MENU_MANAGER);

    await this.runWithLoadedTree('');
    new PanelCommand(PanelCommandType.OPEN_MENUS).send();
    await this.waitForMenu('panel_search_menu');

    let result = await BridgeHelper.sendMessage(TARGET, testToPerform);
    assertEquals('pass', result);
  }
};

AX_TEST_F('ChromeVoxMenuManagerTest', 'ActiveMenu', async function() {
  await this.runTestInPanelManager(
      BridgeConstants.PanelTest.Action.PERFORM_ACTIVE_MENU_TEST);
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'AddMenu', async function() {
  await this.runTestInPanelManager(
      BridgeConstants.PanelTest.Action.PERFORM_ADD_MENU_TEST);
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'AddNodeMenu', async function() {
  await this.runTestInPanelManager(
      BridgeConstants.PanelTest.Action.PERFORM_ADD_MENU_TEST);
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'AdvanceActiveMenuBy', async function() {
  await this.runTestInPanelManager(
      BridgeConstants.PanelTest.Action.PERFORM_ADVANCE_ACTIVE_MENU_BY_TEST);
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'ClearMenus', async function() {
  await this.runTestInPanelManager(
      BridgeConstants.PanelTest.Action.PERFORM_CLEAR_MENUS_TEST);
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'DenySignedOut', async function() {
  await this.runTestInPanelManager(
      BridgeConstants.PanelTest.Action.PERFORM_DENY_SIGNED_OUT_TEST);
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'FindEnabledMenuIndex', async function() {
  await this.runTestInPanelManager(
      BridgeConstants.PanelTest.Action.PERFORM_FIND_ENABLED_MENU_INDEX_TEST);
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'GetSortedKeyBindings', async function() {
  await this.runTestInPanelManager(
      BridgeConstants.PanelTest.Action.PERFORM_GET_SORTED_KEY_BINDINGS_TEST);
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'OnSearchBarQuery', async function() {
  await this.runTestInPanelManager(
      BridgeConstants.PanelTest.Action.PERFORM_ON_SEARCH_BAR_QUERY_TEST);
});
