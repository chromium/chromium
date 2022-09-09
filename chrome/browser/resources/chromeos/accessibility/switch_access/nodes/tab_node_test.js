// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../switch_access_e2e_test_base.js']);

/** Test fixture for the tab node type. */
SwitchAccessTabNodeTest = class extends SwitchAccessE2ETest {
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule(
        'BackButtonNode', '/switch_access/nodes/back_button_node.js');
    await importModule('Navigator', '/switch_access/navigator.js');
    await importModule(
        'SwitchAccessMenuAction', '/switch_access/switch_access_constants.js');
    await importModule('RectUtil', '/common/rect_util.js');
  }
};

TEST_F('SwitchAccessTabNodeTest', 'FindCloseButton', function() {
  this.runWithLoadedDesktop(desktop => {
    const tab = desktop.find({role: chrome.automation.RoleType.TAB});

    // To find the close button, Switch Access relies on it being the only
    // button within a tab.
    let buttonCount = 0;
    for (const child of tab.children) {
      if (child.role === chrome.automation.RoleType.BUTTON) {
        buttonCount++;
      }
    }
    assertEquals(buttonCount, 1);
  });
});

TEST_F('SwitchAccessTabNodeTest', 'Construction', function() {
  this.runWithLoadedDesktop(desktop => {
    Navigator.byItem.moveTo_(
        desktop.find({role: chrome.automation.RoleType.TAB}));

    const tab = Navigator.byItem.node_;
    assertEquals(
        chrome.automation.RoleType.TAB, tab.role, 'Tab node is not a tab');
    assertTrue(tab.isGroup(), 'Tab node should be a group');
    assertEquals(
        1, tab.actions.length, 'Tab as a group should have 1 action (select)');
    assertEquals(
        chrome.accessibilityPrivate.SwitchAccessMenuAction.SELECT,
        tab.actions[0], 'Tab as a group should have the action SELECT');

    Navigator.byItem.node_.doDefaultAction();

    const tabAsRoot = Navigator.byItem.group_;
    assertTrue(
        RectUtil.equal(tab.location, tabAsRoot.location),
        'Tab location should not change when treated as root');
    assertEquals(
        3, tabAsRoot.children.length, 'Tab as root should have 3 children');

    const tabToSelect = Navigator.byItem.node_;
    assertEquals(
        chrome.automation.RoleType.TAB, tabToSelect.role,
        'Tab node to select is not a tab');
    assertFalse(
        tabToSelect.isGroup(), 'Tab node to select should not be a group');
    assertTrue(
        tabToSelect.hasAction(SwitchAccessMenuAction.SELECT),
        'Tab as a group should have a SELECT action');
    assertFalse(
        RectUtil.equal(tabAsRoot.location, tabToSelect.location),
        'Tab node to select should not have the same location as tab as root');
    assertEquals(
        null, tabToSelect.asRootNode(),
        'Tab node to select should not be a root node');

    Navigator.byItem.moveForward();

    const close = Navigator.byItem.node_;
    assertEquals(
        chrome.automation.RoleType.BUTTON, close.role,
        'Close button is not a button');
    assertFalse(close.isGroup(), 'Close button should not be a group');
    assertTrue(
        close.hasAction(SwitchAccessMenuAction.SELECT),
        'Close button should have a SELECT action');
    assertFalse(
        RectUtil.equal(tabAsRoot.location, close.location),
        'Close button should not have the same location as tab as root');
    const overlap = RectUtil.intersection(tabToSelect.location, close.location);
    assertTrue(
        RectUtil.equal(RectUtil.ZERO_RECT, overlap),
        'Close button and tab node to select should not overlap');

    BackButtonNode
        .locationForTesting = {top: 10, left: 10, width: 10, height: 10};
    Navigator.byItem.moveForward();
    assertTrue(
        Navigator.byItem.node_ instanceof BackButtonNode,
        'Third node should be a BackButtonNode');
  });
});
