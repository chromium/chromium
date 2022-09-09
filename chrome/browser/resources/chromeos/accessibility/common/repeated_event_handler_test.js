// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../select_to_speak/select_to_speak_e2e_test_base.js',
]);

/** Test fixture for array_util.js. */
RepeatedEventHandlerTest = class extends SelectToSpeakE2ETest {
  /** @override */
  async setUpDeferred() {
    await importModule(
        'RepeatedEventHandler', '/common/repeated_event_handler.js');
  }
};

AX_TEST_F(
    'RepeatedEventHandlerTest', 'RepeatedEventHandledOnce', async function() {
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
    'RepeatedEventHandlerTest', 'NoEventsHandledAfterStopListening',
    async function() {
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
