// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {FeedbackContext, FeedbackServiceProviderInterface} from './feedback_types.js';

/**
 * @fileoverview
 * Implements a fake version of the FeedbackServiceProvider mojo interface.
 */

/** @implements {FeedbackServiceProviderInterface} */
export class FakeFeedbackServiceProvider {
  constructor() {
    this.methods_ = new FakeMethodResolver();

    // Setup method resolvers.
    this.methods_.register('getFeedbackContext');

    /**
     * Use to track how many times getFeedbackContext has been called.
     * @private {number}
     */
    this.getFeedbackContextCallCount_ = 0;
  }

  /**
   * @returns {number}
   */
  getFeedbackContextCallCount() {
    return this.getFeedbackContextCallCount_;
  }

  /**
   * @return {!Promise<{
   *    feedbackContext: !FeedbackContext,
   *  }>}
   */
  getFeedbackContext() {
    this.getFeedbackContextCallCount_++;
    return this.methods_.resolveMethod('getFeedbackContext');
  }

  /**
   * Sets the value that will be returned when calling getFeedbackContext().
   * @param {!FeedbackContext} feedbackContext
   */
  setFakeFeedbackContext(feedbackContext) {
    this.methods_.setResult(
        'getFeedbackContext', {feedbackContext: feedbackContext});
  }
}
