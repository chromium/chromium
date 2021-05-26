// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['switch_access_e2e_test_base.js']);

/** Test fixture for the point scan manager. */
SwitchAccessPointScanManagerTest = class extends SwitchAccessE2ETest {
  /** @override */
  setUp() {
    var runTest = this.deferRunTest(WhenTestDone.EXPECT);
    (async () => {
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

      runTest();
    })();
  }
};

TEST_F('SwitchAccessPointScanManagerTest', 'PointScanLeftClick', function() {
  const website = '<input type=checkbox style="width: 800px; height: 800px;">';
  this.runWithLoadedTree(website, async (root) => {
    const checkbox = root.find({role: 'checkBox'});
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
