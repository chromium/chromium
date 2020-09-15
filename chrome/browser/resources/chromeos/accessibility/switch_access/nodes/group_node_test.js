// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../switch_access_e2e_test_base.js']);

/** Test fixture for the node wrapper type. */
SwitchAccessGroupNodeTest = class extends SwitchAccessE2ETest {};

TEST_F('SwitchAccessGroupNodeTest', 'NodesRemoved', function() {
  const website = `<button></button>`;
  this.runWithLoadedTree(website, (desktop) => {
    const button = desktop.find({role: chrome.automation.RoleType.BUTTON});
    assertNotEquals(undefined, button);

    const root = new BasicRootNode(desktop);
    assertEquals(0, root.children_.length);

    // Add a group child which has two buttons (same underlying automation
    // node).
    const buttonNode = new BasicNode(button, root);
    const otherButtonNode = new BasicNode(button, root);
    const groupNode = new GroupNode([buttonNode, otherButtonNode]);
    root.children_ = [groupNode];

    // Try asking for the location of the group.
    assertTrue(!!groupNode.location);

    // Try again after clearing one of the button's underlying node.
    buttonNode.baseNode_ = undefined;
    assertTrue(!!groupNode.location);
  });
});
