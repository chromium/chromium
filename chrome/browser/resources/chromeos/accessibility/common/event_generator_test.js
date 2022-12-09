// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['testing/common_e2e_test_base.js']);

/** Test fixture for array_util.js. */
AccessibilityExtensionEventGeneratorTest = class extends CommonE2ETestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule('EventGenerator', '/common/event_generator.js');
  }
};

TEST_F(
    'AccessibilityExtensionEventGeneratorTest',
    'MouseEventsProcessedSequentially', function() {
      const mouseEventLog = [];
      chrome.accessibilityPrivate.sendSyntheticMouseEvent = event =>
          mouseEventLog.push(event);

      const LEFT = chrome.accessibilityPrivate.SyntheticMouseEventButton.LEFT;

      // Set a 1ms delay so that a timeout is set between the press and release.
      EventGenerator.sendMouseClick(100, 100, {delayMs: 1, mouseButton: LEFT});
      assertEquals(
          1, mouseEventLog.length, 'First event should be synchronous');

      EventGenerator.sendMouseClick(200, 200, {delayMs: 1, mouseButton: LEFT});
      assertEquals(
          1, mouseEventLog.length,
          'Second mouse click shouldn\'t start until first has finished');

      const checkEventLog = () => {
        assertEquals(
            4, mouseEventLog.length,
            'Both click events should have completed.');
        assertEquals('press', mouseEventLog[0].type);
        assertEquals('release', mouseEventLog[1].type);
        assertEquals(mouseEventLog[0].x, mouseEventLog[1].x);
        assertEquals(mouseEventLog[0].y, mouseEventLog[1].y);

        assertEquals('press', mouseEventLog[2].type);
        assertEquals('release', mouseEventLog[3].type);
        assertEquals(mouseEventLog[2].x, mouseEventLog[3].x);
        assertEquals(mouseEventLog[2].y, mouseEventLog[3].y);
      };
      // Experimentally, the code takes 13ms to set all timeouts on a
      // development machine. Wait 150ms to increase stability.
      setTimeout(this.newCallback(checkEventLog), 150);
    });
