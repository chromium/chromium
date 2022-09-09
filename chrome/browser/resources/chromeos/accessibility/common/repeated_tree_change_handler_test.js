// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../select_to_speak/select_to_speak_e2e_test_base.js',
]);

/** Test fixture for array_util.js. */
RepeatedTreeChangeHandlerTest = class extends SelectToSpeakE2ETest {
  /** @override */
  async setUpDeferred() {
    await importModule(
        'RepeatedTreeChangeHandler', '/common/repeated_tree_change_handler.js');
  }
};

TEST_F(
    'RepeatedTreeChangeHandlerTest', 'RepeatedTreeChangeHandledOnce',
    function() {
      this.runWithLoadedDesktop(() => {
        this.handlerCallCount = 0;
        const handler = () => this.handlerCallCount++;

        const repeatedHandler =
            new RepeatedTreeChangeHandler('allTreeChanges', handler);

        // Simulate events being fired.
        repeatedHandler.onChange_();
        repeatedHandler.onChange_();
        repeatedHandler.onChange_();
        repeatedHandler.onChange_();
        repeatedHandler.onChange_();

        // Yield before verifying how many times the handler was called.
        setTimeout(() => assertEquals(this.handlerCallCount, 1), 0);
      });
    });

TEST_F('RepeatedTreeChangeHandlerTest', 'Predicate', function() {
  this.runWithLoadedDesktop(() => {
    this.handlerCallCount = 0;
    const handler = () => this.handlerCallCount++;

    const repeatedHandler = new RepeatedTreeChangeHandler(
        'allTreeChanges', handler, {predicate: c => c.type === 'nodeRemoved'});

    // Simulate events being fired.
    repeatedHandler.onChange_({type: 'nodeAdded'});
    repeatedHandler.onChange_({type: 'nodeAdded'});
    repeatedHandler.onChange_({type: 'nodeAdded'});
    repeatedHandler.onChange_({type: 'nodeRemoved'});
    repeatedHandler.onChange_({type: 'nodeRemoved'});
    repeatedHandler.onChange_({type: 'nodeRemoved'});
    repeatedHandler.onChange_({type: 'nodeRemoved'});

    // Verify that nodes that don't satisfy the predicate aren't added to the
    // change stack.
    assertEquals(repeatedHandler.changeStack_.length, 4);

    // Yield before verifying how many times the handler was called.
    setTimeout(() => assertEquals(this.handlerCallCount, 1), 0);
  });
});
