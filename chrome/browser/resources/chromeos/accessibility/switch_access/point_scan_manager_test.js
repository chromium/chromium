// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['switch_access_e2e_test_base.js']);

/** Test fixture for the point scan manager. */
SwitchAccessPointScanManagerTest = class extends SwitchAccessE2ETest {
  /** @override */
  async setUpDeferred() {
    await importModule(
        'BackButtonNode', '/switch_access/nodes/back_button_node.js');
    await importModule(
        ['BasicNode', 'BasicRootNode'], '/switch_access/nodes/basic_node.js');
    await importModule('SACache', '/switch_access/cache.js');
    await importModule(
        'SwitchAccessPredicate', '/switch_access/switch_access_predicate.js');
    await importModule('Navigator', '/switch_access/navigator.js');
    await importModule('SwitchAccess', '/switch_access/switch_access.js');
    await importModule(
        ['SwitchAccessMenuAction', 'SAConstants'],
        '/switch_access/switch_access_constants.js');
  }
};

TEST_F('SwitchAccessPointScanManagerTest', 'PointScanLeftClick', function() {
  const website = '<input type=checkbox style="width: 800px; height: 800px;">';
  this.runWithLoadedTree(website, async (rootWebArea) => {
    const checkbox = rootWebArea.find({role: 'checkBox'});
    checkbox.doDefault();

    const verifyChecked = checked => resolve => {
      const checkedHandler = event => {
        assertEquals(event.target.checked, String(checked));
        event.target.removeEventListener(
            chrome.automation.EventType.CHECKED_STATE_CHANGED, checkedHandler);
        resolve();
      };
      checkbox.addEventListener(
          chrome.automation.EventType.CHECKED_STATE_CHANGED, checkedHandler);
    };
    await new Promise(verifyChecked(true));

    SwitchAccess.mode = SAConstants.Mode.POINT_SCAN;
    Navigator.byPoint.point_ = {x: 600, y: 600};
    Navigator.byPoint.performMouseAction(SwitchAccessMenuAction.LEFT_CLICK);
    await new Promise(verifyChecked(false));
  });
});

TEST_F('SwitchAccessPointScanManagerTest', 'PointScanRightClick', function() {
  const website = '<p>Kittens r cute</p>';
  this.runWithLoadedTree(website, async (rootWebArea) => {
    const findParams = {role: 'menuItem', attributes: {name: /Back.*/}};
    // Context menu with back button shouldn't exist yet.
    const initialMenuItem = rootWebArea.find(findParams);
    assertEquals(initialMenuItem, null);

    const menuItemLoaded = () => resolve => {
      const observer = treeChange => {
        // Wait for the context menu with the back button to show up.
        const menuItem = treeChange.target.find(findParams);
        if (menuItem !== null) {
          chrome.automation.removeTreeChangeObserver(observer);
          resolve();
        }
      };
      chrome.automation.addTreeChangeObserver('allTreeChanges', observer);
    };

    SwitchAccess.mode = SAConstants.Mode.POINT_SCAN;
    Navigator.byPoint.point_ = {x: 400, y: 400};
    Navigator.byPoint.performMouseAction(SwitchAccessMenuAction.RIGHT_CLICK);
    await new Promise(menuItemLoaded());
  });
});

// Verifies that chrome.accessibilityPrivate.setFocusRings() is not called when
// point scanning is running.
TEST_F('SwitchAccessPointScanManagerTest', 'PointScanNoFocusRings', function() {
  const sleep = () => {
    return new Promise(resolve => setTimeout(resolve, 2 * 1000));
  };

  const site = '<button>Test</button>';
  this.runWithLoadedTree(site, async (rootWebArea) => {
    let setFocusRingsCallCount = 0;
    // Mock this API to track how many times it's called.
    chrome.accessibilityPrivate.setFocusRings = (focusRings) => {
      setFocusRingsCallCount += 1;
    };
    assertEquals(0, setFocusRingsCallCount);
    Navigator.byPoint.start();
    // When point scanning starts, setFocusRings() gets called once to clear
    // the focus rings.
    assertEquals(1, setFocusRingsCallCount);
    // Simulate the page focusing the button.
    const button = rootWebArea.find({role: chrome.automation.RoleType.BUTTON});
    assertNotNullNorUndefined(button);
    button.focus();
    // Allow point scanning to run for 2 seconds and ensure no extra calls to
    // setFocusRings().
    await sleep();
    assertEquals(1, setFocusRingsCallCount);
  });
});
