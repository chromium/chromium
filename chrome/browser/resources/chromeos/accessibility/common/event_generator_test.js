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

  runMovePressDragReleaseTest(button) {
    const MOVE = chrome.accessibilityPrivate.SyntheticMouseEventType.MOVE;
    const DRAG = chrome.accessibilityPrivate.SyntheticMouseEventType.DRAG;
    const PRESS = chrome.accessibilityPrivate.SyntheticMouseEventType.PRESS;
    const RELEASE = chrome.accessibilityPrivate.SyntheticMouseEventType.RELEASE;

    const mouseEventLog = [];
    chrome.accessibilityPrivate.sendSyntheticMouseEvent = event =>
        mouseEventLog.push(event);

    EventGenerator.sendMouseMove(42, 1973);
    assertEquals(1, mouseEventLog.length);
    assertEquals(MOVE, mouseEventLog[0].type);
    assertEquals(42, mouseEventLog[0].x);
    assertEquals(1973, mouseEventLog[0].y);

    assertTrue(EventGenerator.sendMousePress(84, 1973, button));
    assertEquals(2, mouseEventLog.length);
    assertEquals(PRESS, mouseEventLog[1].type);
    assertEquals(button, mouseEventLog[1].mouseButton);
    assertEquals(84, mouseEventLog[1].x);
    assertEquals(1973, mouseEventLog[1].y);

    // Cannot press a second time in a row.
    assertFalse(EventGenerator.sendMousePress(84, 1973, button));
    assertEquals(2, mouseEventLog.length);

    // Move while pressed is a drag with the mouse key used
    // during the press event.
    EventGenerator.sendMouseMove(126, 42);
    assertEquals(3, mouseEventLog.length);
    assertEquals(DRAG, mouseEventLog[2].type);
    assertEquals(button, mouseEventLog[2].mouseButton);
    assertEquals(126, mouseEventLog[2].x);
    assertEquals(42, mouseEventLog[2].y);

    // Release works.
    EventGenerator.sendMouseRelease(127, 43);
    assertEquals(4, mouseEventLog.length);
    assertEquals(RELEASE, mouseEventLog[3].type);
    assertEquals(button, mouseEventLog[3].mouseButton);
    assertEquals(127, mouseEventLog[3].x);
    assertEquals(43, mouseEventLog[3].y);

    // Cannot release when already released.
    assertFalse(EventGenerator.sendMouseRelease(84, 1973));
    assertEquals(4, mouseEventLog.length);

    // Now try a press and then send a click. The click should complete after
    // the press's release.
    assertTrue(EventGenerator.sendMousePress(2, 4, button));
    assertEquals(5, mouseEventLog.length);
    assertEquals(PRESS, mouseEventLog[4].type);
    assertEquals(button, mouseEventLog[4].mouseButton);
    assertEquals(2, mouseEventLog[4].x);
    assertEquals(4, mouseEventLog[4].y);

    EventGenerator.sendMouseClick(200, 200, {delayMs: 1, mouseButton: button});
    assertEquals(5, mouseEventLog.length);

    assertTrue(EventGenerator.sendMouseRelease(4, 8, button));
    // Now the next click should start too.
    assertEquals(7, mouseEventLog.length);
    assertEquals(RELEASE, mouseEventLog[5].type);
    assertEquals(button, mouseEventLog[5].mouseButton);
    assertEquals(4, mouseEventLog[5].x);
    assertEquals(8, mouseEventLog[5].y);

    assertEquals(PRESS, mouseEventLog[6].type);
    assertEquals(button, mouseEventLog[6].mouseButton);
    assertEquals(200, mouseEventLog[6].x);
    assertEquals(200, mouseEventLog[6].y);

    setTimeout(
        this.newCallback(() => {
          assertEquals(8, mouseEventLog.length);
          assertEquals(RELEASE, mouseEventLog[7].type);
          assertEquals(button, mouseEventLog[7].mouseButton);
          assertEquals(200, mouseEventLog[7].x);
          assertEquals(200, mouseEventLog[7].y);
        }),
        150);
  }
};

AX_TEST_F(
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

AX_TEST_F(
    'AccessibilityExtensionEventGeneratorTest', 'MouseMovePressDragReleaseLeft',
    function() {
      this.runMovePressDragReleaseTest(
          chrome.accessibilityPrivate.SyntheticMouseEventButton.LEFT);
    });

AX_TEST_F(
    'AccessibilityExtensionEventGeneratorTest',
    'MouseMovePressDragReleaseRight', function() {
      this.runMovePressDragReleaseTest(
          chrome.accessibilityPrivate.SyntheticMouseEventButton.RIGHT);
    });
