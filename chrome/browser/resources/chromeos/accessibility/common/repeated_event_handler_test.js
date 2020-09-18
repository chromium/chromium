// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../chromevox/testing/chromevox_next_e2e_test_base.js',
  'repeated_event_handler.js'
]);

/** Test fixture for array_util.js. */
RepeatedEventHandlerTest = class extends ChromeVoxNextE2ETest {};

TEST_F('RepeatedEventHandlerTest', 'RepeatedEventHandledOnce', function() {
  this.runWithLoadedTree('', (root) => {
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
});

TEST_F(
    'RepeatedEventHandlerTest', 'NoEventsHandledAfterStopListening',
    function() {
      this.runWithLoadedTree('', (root) => {
        this.handlerCallCount = 0;
        const handler = () => this.handlerCallCount++;

        const repeatedHandler =
            new RepeatedEventHandler(root, 'focus', handler);

        // Simulate events being fired.
        repeatedHandler.onEvent_();
        repeatedHandler.onEvent_();
        repeatedHandler.onEvent_();

        repeatedHandler.stop();

        // Yield before verifying how many times the handler was called.
        setTimeout(
            this.newCallback(() => assertEquals(this.handlerCallCount, 0)), 0);
      });
    });
