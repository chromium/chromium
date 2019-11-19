// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Creates wrappers for callbacks and calls testDone() when all callbacks
 * have been invoked.
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
  wrap: function(opt_callback) {
    var callback = opt_callback || function() {};
    var savedArgs = new SaveMockArguments();
    var lastCall = null;
    var completionAction = callFunctionWithSavedArgs(savedArgs, function() {
      if (lastCall) {
        throw new Error('Called more than once, first call here: ' + lastCall);
      } else {
        lastCall = new Error().stack;
      }
      callback.apply(this.fixture_, arguments);
      if (--this.pendingCallbacks_ <= 0) {
        CallbackHelper.testDone_();
      }
    }.bind(this));
    // runAllActionsAsync catches exceptions and puts them in the test
    // framework's list of errors and fails the test if appropriate.
    var runAll = runAllActionsAsync(WhenTestDone.ASSERT, completionAction);
    ++this.pendingCallbacks_;
    return function() {
      savedArgs.arguments = Array.prototype.slice.call(arguments);
      runAll.invoke();
    };
  }
};

/**
 * @private
 */
CallbackHelper.testDone_ = this.testDone;
// Remove testDone for public use since directly using it conflicts with
// this callback helper.
delete this.testDone;
