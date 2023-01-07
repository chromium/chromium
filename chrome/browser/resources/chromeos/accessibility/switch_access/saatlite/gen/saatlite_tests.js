// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../switch_access_e2e_test_base.js', '../../test_utility.js']);

/** Test fixture for the SAATLite generated tests. */
SwitchAccessSAATLiteTest = class extends SwitchAccessE2ETest {
  /** @override */
  async setUpDeferred() {
    await TestUtility.setup();
  }
};

TEST_F('SwitchAccessSAATLiteTest', 'Demo', function() {
  this.runWithLoadedTree('<button>Hi</button>', async rootWebArea => {
    TestUtility.startFocusInside(rootWebArea);
    TestUtility.pressNextSwitch();
    TestUtility.pressPreviousSwitch();
    TestUtility.pressSelectSwitch();
    await TestUtility.expectFocusOn({role: 'button'});
  });
});
