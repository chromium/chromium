// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['testing/common_e2e_test_base.js']);

/** Test fixture for array_util.js. */
AccessibilityExtensionRepeatedEventHandlerTest =
    class extends CommonE2ETestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    await Promise.all([
      importModule('EventGenerator', '/common/event_generator.js'),
      importModule('KeyCode', '/common/key_code.js'),
      importModule('RepeatedEventHandler', '/common/repeated_event_handler.js'),
    ]);
  }
};

AX_TEST_F(
    'AccessibilityExtensionRepeatedEventHandlerTest',
    'RepeatedEventHandledOnce', async function() {
      const root = await this.runWithLoadedTree('');
      this.handlerCallCount = 0;
      const handler = () => this.handlerCallCount++;

      const repeatedHandler = new RepeatedEventHandler(root, 'focus', handler);

      // Simulate events being fired.
      repeatedHandler.onEvent_();
      repeatedHandler.onEvent_();
      repeatedHandler.onEvent_();
      repeatedHandler.onEvent_();
      repeatedHandler.onEvent_();

      // Yield before verify how many times the handler was called.
      setTimeout(
          this.newCallback(() => assertEquals(this.handlerCallCount, 1)), 0);
    });

AX_TEST_F(
    'AccessibilityExtensionRepeatedEventHandlerTest',
    'NoEventsHandledAfterStopListening', async function() {
      const root = await this.runWithLoadedTree('');
      this.handlerCallCount = 0;
      const handler = () => this.handlerCallCount++;

      const repeatedHandler = new RepeatedEventHandler(root, 'focus', handler);

      // Simulate events being fired.
      repeatedHandler.onEvent_();
      repeatedHandler.onEvent_();
      repeatedHandler.onEvent_();

      repeatedHandler.stop();

      // Yield before verifying how many times the handler was called.
      setTimeout(
          this.newCallback(() => assertEquals(this.handlerCallCount, 0)), 0);
    });
