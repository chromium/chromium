// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {FeedbackContext, FeedbackServiceProviderInterface, Report, SendReportStatus} from './feedback_types.js';

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
    this.methods_.register('sendReport');

    /**
     * Use to track how many times getFeedbackContext has been called.
     * @private {number}
     */
    this.getFeedbackContextCallCount_ = 0;
  }

  /**
   * @return {number}
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
   * @param {!Report} report
   * @return {!Promise<{
   *    status: !SendReportStatus,
   *  }>}
   */
  sendReport(report) {
    return this.methods_.resolveMethod('sendReport');
  }

  /**
   * Sets the value that will be returned when calling getFeedbackContext().
   * @param {!FeedbackContext} feedbackContext
   */
  setFakeFeedbackContext(feedbackContext) {
    this.methods_.setResult(
        'getFeedbackContext', {feedbackContext: feedbackContext});
  }
  /**
   * Sets the value that will be returned when calling sendReport().
   * @param {!SendReportStatus} status
   */
  setFakeSendFeedbackStatus(status) {
    this.methods_.setResult('sendReport', {status: status});
  }
}
