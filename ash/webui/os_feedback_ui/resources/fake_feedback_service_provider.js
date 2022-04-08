// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {FeedbackServiceProviderInterface} from './feedback_types.js';

/**
 * @fileoverview
 * Implements a fake version of the FeedbackServiceProvider mojo interface.
 */

/** @implements {FeedbackServiceProviderInterface} */
export class FakeFeedbackServiceProvider {
  constructor() {
    this.methods_ = new FakeMethodResolver();

    // Setup method resolvers.
    this.methods_.register('getUserEmail');

    /**
     * Use to track how many times getUserEmail has been called.
     * @private {number}
     */
    this.getUserEmailCallCount_ = 0;
  }

  /**
   * @returns {number}
   */
  getUserEmailCallCount() {
    return this.getUserEmailCallCount_;
  }

  /**
   * @return {!Promise<{email: !string}>}
   */
  getUserEmail() {
    this.getUserEmailCallCount_++;
    return this.methods_.resolveMethod('getUserEmail');
  }

  /**
   * Sets the value that will be returned when calling getUserEmail().
   * @param {!string} email
   */
  setFakeEmail(email) {
    this.methods_.setResult('getUserEmail', {email: email});
  }
}
