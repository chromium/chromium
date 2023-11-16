// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {FeedbackAppExitPath, FeedbackAppHelpContentOutcome, FeedbackAppPostSubmitAction, FeedbackAppPreSubmitAction, FeedbackContext, FeedbackServiceProviderInterface, Report, SendReportStatus} from './os_feedback_ui.mojom-webui.js';

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
    this.methods_.register('getScreenshotPng');
    this.methods_.register('sendReport');
    // Let sendReport return success by default.
    this.methods_.setResult('sendReport', {status: SendReportStatus.kSuccess});
    // Let getScreenshotPng return an empty array by default.
    this.methods_.setResult('getScreenshotPng', {pngData: []});

    /**
     * Used to track how many times each method is being called.
     * @private
     */
    this.callCounts_ = {
      /** @type {number} */
      getFeedbackContext: 0,
      /** @type {number} */
      getScreenshotPng: 0,
      /** @type {number} */
      sendReport: 0,
      /** @type {number} */
      openDiagnosticsApp: 0,
      /** @type {number} */
      openExploreApp: 0,
      /** @type {number} */
      openMetricsDialog: 0,
      /** @type {number} */
      openSystemInfoDialog: 0,
      /** @type {number} */
      openAutofillDialog: 0,
      /** @type {number} */
      recordHelpContentSearchResultCount: 0,
    };

    /** @type {?FeedbackAppPostSubmitAction} */
    this.postSubmitAction_ = null;

    /** @type {?FeedbackAppExitPath} */
    this.exitPath_ = null;

    /** @type {?FeedbackAppHelpContentOutcome} */
    this.helpContentOutcome_ = null;

    /** @type {Map<FeedbackAppPreSubmitAction, number>} */
    this.preSubmitActionMap_ = new Map();
  }

  /**
   * @return {number}
   */
  getFeedbackContextCallCount() {
    return this.callCounts_.getFeedbackContext;
  }

  /**
   * @return {!Promise<{
   *    feedbackContext: !FeedbackContext,
   *  }>}
   */
  getFeedbackContext() {
    this.callCounts_.getFeedbackContext++;
    return this.methods_.resolveMethod('getFeedbackContext');
  }

  /**
   * @return {number}
   */
  getSendReportCallCount() {
    return this.callCounts_.sendReport;
  }

  /**
   * @param {!Report} report
   * @return {!Promise<{
   *    status: !SendReportStatus,
   *  }>}
   */
  sendReport(report) {
    this.callCounts_.sendReport++;
    return this.methods_.resolveMethod('sendReport');
  }

  /**
   * @return {number}
   */
  getScreenshotPngCallCount() {
    return this.callCounts_.getScreenshotPng;
  }

  /**
   * @return {!Promise<{
   *    pngData: !Array<!number>,
   * }>}
   */
  getScreenshotPng() {
    this.callCounts_.getScreenshotPng++;
    return this.methods_.resolveMethod('getScreenshotPng');
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

  /**
   * Sets the value that will be returned when calling getScreenshotPng().
   * @param {!Array<!number>} data
   */
  setFakeScreenshotPng(data) {
    this.methods_.setResult('getScreenshotPng', {pngData: data});
  }

  /**
   * @return {number}
   */
  getOpenDiagnosticsAppCallCount() {
    return this.callCounts_.openDiagnosticsApp;
  }

  openDiagnosticsApp() {
    this.callCounts_.openDiagnosticsApp++;
  }

  /**
   * @return {number}
   */
  getOpenExploreAppCallCount() {
    return this.callCounts_.openExploreApp;
  }

  openExploreApp() {
    this.callCounts_.openExploreApp++;
  }

  /**
   * @return {number}
   */
  getOpenMetricsDialogCallCount() {
    return this.callCounts_.openMetricsDialog;
  }

  openMetricsDialog() {
    this.callCounts_.openMetricsDialog++;
  }

  /**
   * @return {number}
   */
  getOpenSystemInfoDialogCallCount() {
    return this.callCounts_.openSystemInfoDialog;
  }

  openSystemInfoDialog() {
    this.callCounts_.openSystemInfoDialog++;
  }

  /**
   * @return {number}
   */
  getOpenAutofillDialogCallCount() {
    return this.callCounts_.openAutofillDialog;
  }

  openAutofillDialog() {
    this.callCounts_.openAutofillDialog++;
  }

  /**
   * @return {number}
   */
  getRecordHelpContentSearchResultCount() {
    return this.callCounts_.recordHelpContentSearchResultCount;
  }

  recordHelpContentSearchResultCount() {
    this.callCounts_.recordHelpContentSearchResultCount++;
  }

  /**
   * @param {!FeedbackAppPostSubmitAction} action
   * @return {boolean}
   */
  isRecordPostSubmitActionCalled(action) {
    return this.postSubmitAction_ === action;
  }

  /**
   * @param {!FeedbackAppPostSubmitAction} action
   */
  recordPostSubmitAction(action) {
    if (this.postSubmitAction_ === null) {
      this.postSubmitAction_ = action;
    }
  }

  /**
   * @param {?FeedbackAppExitPath} exitPath
   * @return {boolean}
   */
  isRecordExitPathCalled(exitPath) {
    return this.exitPath_ === exitPath;
  }

  /**
   * @param {?FeedbackAppExitPath} exitPath
   */
  recordExitPath(exitPath) {
    if (this.exitPath_ === null) {
      this.exitPath_ = exitPath;
    }
  }

  /**
   * @param {!FeedbackAppPreSubmitAction} action
   * @return {number}
   */
  getRecordPreSubmitActionCallCount(action) {
    return this.preSubmitActionMap_.get(action) || 0;
  }

  /**
   * @param {!FeedbackAppPreSubmitAction} action
   */
  recordPreSubmitAction(action) {
    this.preSubmitActionMap_.set(
        action, this.preSubmitActionMap_.get(action) + 1 || 1);
  }

  /**
   * @param {?FeedbackAppHelpContentOutcome} outcome
   * @return {boolean}
   */
  isHelpContentOutcomeMetricEmitted(outcome) {
    return this.helpContentOutcome_ === outcome;
  }

  /**
   * @param {?FeedbackAppHelpContentOutcome} outcome
   */
  recordHelpContentOutcome(outcome) {
    assert(this.helpContentOutcome_ === null);
    this.helpContentOutcome_ = outcome;
  }
}
