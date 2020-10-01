// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['switch_access_e2e_test_base.js']);

/** Test fixture for the menu manager. */
SwitchAccessMenuManagerTest = class extends SwitchAccessE2ETest {
  openMenuForTextField(desktop) {
    const textField = desktop.find({role: 'textField'});
    // We expect there to be at least one text field onscreen (the omnibar).
    assertNotNullNorUndefined(textField, 'Couldn\'t find a text field');
    NavigationManager.instance.moveTo_(textField);
    MenuManager.enter();

    assertTrue(
        MenuManager.instance.isMenuOpen_,
        'Menu manager should be marked as open');
    assertTrue(
        MenuManager.instance.actionNode_.isEquivalentTo(textField),
        'Menu is open for the wrong node');
  }

  // Callback will be called when the menu has loaded, and Switch Access focus
  // has shifted to the menu.
  setMenuLoadCallback(callback) {
    MenuManager.instance.onMenuLoadedForTesting_ = this.newCallback(callback);
  }

  waitForMenuClose(callback) {
    const closedPredicate = () => {
      const node = NavigationManager.desktopNode.find(
          {role: 'menu', attributes: {className: 'SwitchAccessMenuView'}});
      if (!node || !node.role) {
        return true;
      }
      if (node.state['offscreen'] || node.state['invisible']) {
        return true;
      }
      if (node.location.width === 0 && node.location.height === 0) {
        return true;
      }
      return false;
    };
    this.waitForPredicate(closedPredicate, this.newCallback(callback));
  }
};

TEST_F('SwitchAccessMenuManagerTest', 'Enter', function() {
  this.runWithLoadedTree('', (desktop) => {
    const manager = MenuManager.instance;

    this.setMenuLoadCallback(() => {
      assertFalse(
          manager.inTextNavigation_, 'Menu should not be in text navigation');
      assertNotNullNorUndefined(manager.actionNode_, 'Menu has no action node');
      const actionNode = manager.actionNode_;
      assertEquals(
          actionNode.automationNode.role, 'textField',
          'Menu is not open for the textField');
      assertGT(
          actionNode.actions.length, 1,
          'TextField should have more than 1 action available');

      const menuNode = manager.menuAutomationNode_;
      assertTrue(
          RectUtil.close(
              menuNode.location, manager.displayedLocation_, /*tolerance=*/ 10),
          'Menu should be close to the display location');
      assertGT(menuNode.location.width, 0, 'Menu should have a nonzero width');
      assertGT(
          menuNode.location.height, 0, 'Menu should have a nonzero height');
      assertFalse(
          !!menuNode.state['offscreen'],
          'Menu should not be marked as offscreen');

      const interestingChildren =
          BasicRootNode.getInterestingChildren(menuNode);
      const globalActionCount = 1;
      assertEquals(
          actionNode.actions.length + globalActionCount,
          interestingChildren.length,
          'Menu should show all actions for textField (while improved text ' +
              'navigation flag is disabled)');
      for (let i = 0; i < actionNode.actions.length; i++) {
        action = actionNode.actions[i];
        button = interestingChildren[i].value;
        assertEquals(
            action, button,
            'Button ' + i + ' ("' + button + '") should be action "' + action +
                '"');
      }
    });

    this.openMenuForTextField(desktop);
  });
});

TEST_F('SwitchAccessMenuManagerTest', 'Exit', function() {
  this.runWithLoadedTree('', (desktop) => {
    const manager = MenuManager.instance;

    this.setMenuLoadCallback(() => {
      MenuManager.exit();
      assertFalse(manager.isMenuOpen_, 'Menu should be marked as closed');
      assertFalse(
          manager.inTextNavigation_, 'Menu should not be in text navigation');
      assertNullOrUndefined(
          manager.actionNode_, 'Action node should have been reset');
      assertNullOrUndefined(
          manager.displayedActions_,
          'Displayed actions should have been reset');
      assertNullOrUndefined(
          manager.displayedLocation_,
          'Displayed location should have been reset');
      const navGroup = NavigationManager.instance.group_.automationNode;
      assertNotEquals(
          navGroup.className, 'SwitchAccessMenuView',
          'Navigation manager did not exit the menu');

      this.waitForMenuClose();
    });

    this.openMenuForTextField(desktop);
  });
});

TEST_F('SwitchAccessMenuManagerTest', 'Navigation', function() {
  const website = `<button id="test" aria-pressed=false>First Button</button>
      <script>
        let state = false;
        let button = document.getElementById('test');
        button.onclick = () => {
          state = !state;
          button.setAttribute('aria-pressed', state);
        };
      </script>`;
  this.runWithLoadedTree(website, (desktop) => {
    const manager = MenuManager.instance;
    const navigator = NavigationManager.instance;

    const button = this.findNodeById('test');
    button.addEventListener('checkedStateChanged', this.newCallback((event) => {
      assertEquals(
          button.htmlAttributes.id, event.target.htmlAttributes.id,
          'Checked state changed on unexpected node');
    }));
    navigator.moveTo_(button);

    this.setMenuLoadCallback(() => {
      assertTrue(
          navigator.group_.isEquivalentTo(manager.menuAutomationNode_),
          'Navigation should be focused on the menu');
      const selectButton = navigator.node_;
      assertEquals(
          'select', selectButton.automationNode.value,
          'The first action in the menu should be select');
      NavigationManager.moveForward();
      assertEquals(
          'settings', navigator.node_.automationNode.value,
          'The second action in the menu should be settings');
      NavigationManager.moveForward();
      assertTrue(
          navigator.node_ instanceof BackButtonNode,
          'The third element in the menu should be the back button');
      NavigationManager.moveForward();
      assertTrue(
          selectButton.equals(navigator.node_),
          'Moving forward from the back button should take us to the first ' +
              'action (select)');
      // Press the select button.
      MenuManager.enter();

      // Wait for the menu to close
      this.waitForMenuClose(() => {
        assertFalse(manager.isMenuOpen_);
      });
    });

    // Force the menu to show with different actions than it would normally.
    manager.actionNode_ = NavigationManager.currentNode;
    manager.displayMenuWithActions_(['select', 'settings']);
  });
});
