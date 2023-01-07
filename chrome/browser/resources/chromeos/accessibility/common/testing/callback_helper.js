// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Creates wrappers for callbacks and calls testDone() when all callbacks
 * have been invoked. Callbacks may return a promise to defer completion and
 * continued processing of subsequent callbacks.
 * @param {testing.Test} fixture
 */
function CallbackHelper(fixture) {
  /** @type {Object} fixture */
  this.fixture_ = fixture;
  /** @type {number} */
  this.pendingCallbacks_ = 0;
}

CallbackHelper.prototype = {
  /**
   * @param {Function=} opt_callback
   * @return {Function}
   */
  wrap(opt_callback) {
    const callback = opt_callback || function() {};
    const savedArgs = new SaveMockArguments();
    let lastCall = null;
    const completionAction = callFunctionWithSavedArgs(savedArgs, function() {
      if (lastCall) {
        throw new Error('Called more than once, first call here: ' + lastCall);
      } else {
        lastCall = new Error().stack;
      }
      const result = callback.apply(this.fixture_, arguments);
      if (result) {
        if (!(result instanceof Promise)) {
          throw new Error('Only support return type of Promise');
        }
        result
            .then(
                () => {
                  if (--this.pendingCallbacks_ <= 0) {
                    CallbackHelper.testDone_();
                  }
                },
                reason => {
                  CallbackHelper.testDone_([false, reason.toString()]);
                })
            .catch(reason => {
              CallbackHelper.testDone_([false, reason.toString()]);
            });
      } else {
        if (--this.pendingCallbacks_ <= 0) {
          CallbackHelper.testDone_();
        }
      }
    }.bind(this));
    // runAllActionsAsync catches exceptions and puts them in the test
    // framework's list of errors and fails the test if appropriate.
    const runAll = runAllActionsAsync(WhenTestDone.ASSERT, completionAction);
    ++this.pendingCallbacks_;
    return function() {
      savedArgs.arguments = Array.prototype.slice.call(arguments);
      runAll.invoke();
    };
  },
};

/**
 * @private
 */
CallbackHelper.testDone_ = this.testDone;
// Remove testDone for public use since directly using it conflicts with
// this callback helper.
delete this.testDone;
