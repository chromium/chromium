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
};

AX_TEST_F('ChromeVoxMenuManagerTest', 'ActiveMenu', async function() {
  await this.runWithLoadedTree('');
  new PanelCommand(PanelCommandType.OPEN_MENUS).send();
  await this.waitForMenu('panel_search_menu');

  const menuManager = this.getMenuManager();
  const searchMenu = menuManager.activeMenu_;
  const chromevoxMenu = menuManager.getSelectedMenu('panel_menu_chromevox');
  assertEquals('panel_menu_chromevox', chromevoxMenu.menuMsg);

  // Set a pending callback so we can tell if it was cleared.
  this.getPanel().instance.setPendingCallback(() => {});
  assertNotNullNorUndefined(this.getPanel().instance.pendingCallback_);

  menuManager.activateMenu(chromevoxMenu);
  assertEquals(chromevoxMenu, menuManager.activeMenu_);
  assertEquals(-1, searchMenu.activeIndex_);
  // Confirm the pending callback was cleared.
  assertNullOrUndefined(this.getPanel().instance.pendingCallback_);
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'AddMenu', async function() {
  await this.runWithLoadedTree('');
  new PanelCommand(PanelCommandType.OPEN_MENUS).send();
  await this.waitForMenu('panel_search_menu');
  const searchMenu = this.getMenuManager().activeMenu_;

  // Msgs.getMsg() throws an error if the message is not a translated string.
  // Mock it out to avoid the error.
  this.getPanelWindow().Msgs.getMsg = () => '';

  // Confirm that |fake_menu| is not in the set of menus.
  assertFalse(
      this.getMenuManager().menus_.some(menu => menu.menuMsg === 'fake_menu'));

  const fakeMenu = this.getMenuManager().addMenu('fake_menu');
  assertEquals('fake_menu', fakeMenu.menuMsg);
  // Confirm that |fake_menu| is now in the menu list.
  assertTrue(
      this.getMenuManager().menus_.some(menu => menu.menuMsg === 'fake_menu'));
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'AddNodeMenu', async function() {
  await this.runWithLoadedTree('');
  new PanelCommand(PanelCommandType.OPEN_MENUS).send();
  await this.waitForMenu('panel_search_menu');

  // Msgs.getMsg() throws an error if the message is not a translated string.
  // Mock it out to avoid the error.
  this.getPanelWindow().Msgs.getMsg = () => '';

  const data = {menuId: 6, titleId: 'fake_node'};
  this.getMenuManager().addNodeMenu(data);
  const fakeMenu = this.getMenuManager().getSelectedMenu('fake_node');

  // Ensure we didn't receive the default menu.
  assertEquals('fake_node', fakeMenu.menuMsg);
  assertEquals(fakeMenu, this.getMenuManager().nodeMenuDictionary_[6]);
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'AdvanceActiveMenuBy', async function() {
  await this.runWithLoadedTree('');
  new PanelCommand(PanelCommandType.OPEN_MENUS).send();
  await this.waitForMenu('panel_search_menu');

  const menuManager = this.getMenuManager();
  const activeIndex = () =>
      menuManager.menus_.findIndex(menu => menu === menuManager.activeMenu_);
  assertEquals(0, activeIndex());

  // Forward with an active menu.
  menuManager.advanceActiveMenuBy(2);
  assertEquals(2, activeIndex());

  // Backward with an active menu.
  menuManager.advanceActiveMenuBy(-1);
  assertEquals(1, activeIndex());

  // Wrap around backwards.
  menuManager.advanceActiveMenuBy(-3);
  assertEquals(menuManager.menus_.length - 2, activeIndex());

  // Wrap around forwards.
  menuManager.advanceActiveMenuBy(4);
  assertEquals(2, activeIndex());

  // Forward with no active menu.
  menuManager.activeMenu_ = null;
  assertEquals(-1, activeIndex());

  menuManager.advanceActiveMenuBy(1);
  assertEquals(0, activeIndex());

  // Backward with no active menu.
  menuManager.activeMenu_ = null;
  assertEquals(-1, activeIndex());

  menuManager.advanceActiveMenuBy(-1);
  assertEquals(menuManager.menus_.length - 1, activeIndex());
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'ClearMenus', async function() {
  await this.runWithLoadedTree('');
  new PanelCommand(PanelCommandType.OPEN_MENUS).send();
  await this.waitForMenu('panel_search_menu');

  const panelDoc = this.getPanelWindow().document;
  const menuBar = panelDoc.getElementById('menu-bar');
  assertNotEquals(0, menuBar.children.length);
  assertEquals('', this.getMenuManager().lastMenu_);
  assertNotEquals(null, this.getMenuManager().activeMenu_);

  this.getMenuManager().clearMenus();

  assertEquals(0, menuBar.children.length);
  assertEquals('panel_search_menu', this.getMenuManager().lastMenu_);
  assertEquals(null, this.getMenuManager().activeMenu_);
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'DenySignedOut', async function() {
  await this.runWithLoadedTree('');
  new PanelCommand(PanelCommandType.OPEN_MENUS).send();
  await this.waitForMenu('panel_search_menu');

  const menuManager = this.getMenuManager();
  const someItemsAreDisabled = () =>
      menuManager.menus_.some(menu => menu.items.some(item => !item.enabled_));

  assertFalse(someItemsAreDisabled());
  menuManager.denySignedOut();
  assertTrue(someItemsAreDisabled());
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'FindEnabledMenuIndex', async function() {
  await this.runWithLoadedTree('');
  new PanelCommand(PanelCommandType.OPEN_MENUS).send();
  await this.waitForMenu('panel_search_menu');

  const menuManager = this.getMenuManager();

  // Try forward and backward when all menus are enabled.
  let index = menuManager.findEnabledMenuIndex(0, 1);
  assertEquals(0, index);

  index = menuManager.findEnabledMenuIndex(1, -1);
  assertEquals(1, index);

  // Try forward and backward when no menus are enabled.
  menuManager.menus_.forEach(menu => menu.enabled_ = false);

  index = menuManager.findEnabledMenuIndex(2, 1);
  assertEquals(-1, index);

  index = menuManager.findEnabledMenuIndex(2, -1);
  assertEquals(-1, index);

  // Try forward and backward when one menu is enabled in that direction.
  menuManager.menus_[2].enabled_ = true;

  index = menuManager.findEnabledMenuIndex(0, 1);
  assertEquals(2, index);

  index = menuManager.findEnabledMenuIndex(menuManager.menus_.length - 1, -1);
  assertEquals(2, index);

  // Try forward and backward when there is one enabled menu, but in the
  // opposite direction.
  index = menuManager.findEnabledMenuIndex(3, 1);
  assertEquals(-1, index);

  index = menuManager.findEnabledMenuIndex(1, -1);
  assertEquals(-1, index);
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'GetSortedKeyBindings', async function() {
  await this.runWithLoadedTree('');
  new PanelCommand(PanelCommandType.OPEN_MENUS).send();
  await this.waitForMenu('panel_search_menu');

  const keymapBindings = KeyMap.get().bindings();
  const sortedKeyBindings = await this.getMenuManager().getSortedKeyBindings();
  assertEquals(keymapBindings.length, sortedKeyBindings.length);

  for (const binding of sortedKeyBindings) {
    assertNotEquals('', binding.command);
    assertEquals('string', typeof binding.keySeq);
    assertEquals('string', typeof binding.title);
    assertTrue(keymapBindings.some(
        keyBinding => binding.sequence.equals(keyBinding.sequence)));
  }
});

AX_TEST_F('ChromeVoxMenuManagerTest', 'OnSearchBarQuery', async function() {
  await this.runWithLoadedTree('');
  new PanelCommand(PanelCommandType.OPEN_MENUS).send();
  await this.waitForMenu('panel_search_menu');

  const searchMenu = this.getMenuManager().searchMenu_;

  const expectSearchMenuCleared =
      this.prepareToExpectMethodCall(searchMenu, 'clear');
  const expectActivateMenu =
      this.prepareToExpectMethodCall(this.getMenuManager(), 'activateMenu');

  this.getMenuManager().onSearchBarQuery({target: {value: 'a'}});
  await this.waitForPendingMethods();
  expectSearchMenuCleared();
  expectActivateMenu();
  assertNotEquals(0, searchMenu.items.length);
});
