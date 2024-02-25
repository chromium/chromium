// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {assert} from 'chrome://resources/js/assert.js';

import {FeedbackAppExitPath, FeedbackAppHelpContentOutcome, FeedbackAppPostSubmitAction, FeedbackAppPreSubmitAction, FeedbackContext, FeedbackServiceProviderInterface, Report, SendReportStatus} from './os_feedback_ui.mojom-webui.js';

/**
 * @fileoverview
 * Implements a fake version of the FeedbackServiceProvider mojo interface.
 */

export class FakeFeedbackServiceProvider implements
    FeedbackServiceProviderInterface {
  private exitPath: FeedbackAppExitPath|null = null;
  private helpContentOutcome: FeedbackAppHelpContentOutcome|null = null;
  private postSubmitAction: FeedbackAppPostSubmitAction|null = null;
  private preSubmitActionMap: Map<FeedbackAppPreSubmitAction, number> =
      new Map();
  private methods: FakeMethodResolver;
  /** Used to track how many times each method is being called.*/
  private callCounts = {
    getFeedbackContext: 0,
    getScreenshotPng: 0,
    sendReport: 0,
    openDiagnosticsApp: 0,
    openExploreApp: 0,
    openMetricsDialog: 0,
    openSystemInfoDialog: 0,
    openAutofillDialog: 0,
    recordHelpContentSearchResultCount: 0,
  };

  constructor() {
    this.methods = new FakeMethodResolver();
    // Setup method resolvers.
    this.methods.register('getFeedbackContext');
    this.methods.register('getScreenshotPng');
    this.methods.register('sendReport');
    // Let sendReport return success by default.
    this.methods.setResult('sendReport', {status: SendReportStatus.kSuccess});
    // Let getScreenshotPng return an empty array by default.
    this.methods.setResult('getScreenshotPng', {pngData: []});
  }

  getFeedbackContextCallCount(): number {
    return this.callCounts.getFeedbackContext;
  }

  getFeedbackContext(): Promise<{feedbackContext: FeedbackContext}> {
    this.callCounts.getFeedbackContext++;
    return this.methods.resolveMethod('getFeedbackContext');
  }

  getSendReportCallCount(): number {
    return this.callCounts.sendReport;
  }

  sendReport(_report: Report): Promise<{status: SendReportStatus}> {
    this.callCounts.sendReport++;
    return this.methods.resolveMethod('sendReport');
  }

  getScreenshotPngCallCount(): number {
    return this.callCounts.getScreenshotPng;
  }

  getScreenshotPng(): Promise<{pngData: number[]}> {
    this.callCounts.getScreenshotPng++;
    return this.methods.resolveMethod('getScreenshotPng');
  }

  /** Sets the value that will be returned when calling getFeedbackContext(). */
  setFakeFeedbackContext(feedbackContext: FeedbackContext) {
    this.methods.setResult('getFeedbackContext', {feedbackContext});
  }

  /**  Sets the value that will be returned when calling sendReport(). */
  setFakeSendFeedbackStatus(status: SendReportStatus) {
    this.methods.setResult('sendReport', {status});
  }

  /**  Sets the value that will be returned when calling getScreenshotPng(). */
  setFakeScreenshotPng(data: number[]) {
    this.methods.setResult('getScreenshotPng', {pngData: data});
  }

  getOpenDiagnosticsAppCallCount(): number {
    return this.callCounts.openDiagnosticsApp;
  }

  openDiagnosticsApp() {
    this.callCounts.openDiagnosticsApp++;
  }

  getOpenExploreAppCallCount(): number {
    return this.callCounts.openExploreApp;
  }

  openExploreApp() {
    this.callCounts.openExploreApp++;
  }

  getOpenMetricsDialogCallCount(): number {
    return this.callCounts.openMetricsDialog;
  }

  openMetricsDialog() {
    this.callCounts.openMetricsDialog++;
  }

  getOpenSystemInfoDialogCallCount(): number {
    return this.callCounts.openSystemInfoDialog;
  }

  openSystemInfoDialog() {
    this.callCounts.openSystemInfoDialog++;
  }

  getOpenAutofillDialogCallCount(): number {
    return this.callCounts.openAutofillDialog;
  }

  openAutofillDialog() {
    this.callCounts.openAutofillDialog++;
  }

  getRecordHelpContentSearchResultCount(): number {
    return this.callCounts.recordHelpContentSearchResultCount;
  }

  recordHelpContentSearchResultCount() {
    this.callCounts.recordHelpContentSearchResultCount++;
  }

  isRecordPostSubmitActionCalled(action: FeedbackAppPostSubmitAction): boolean {
    return this.postSubmitAction !== null && this.postSubmitAction === action;
  }

  recordPostSubmitAction(action: FeedbackAppPostSubmitAction) {
    if (this.postSubmitAction === null) {
      this.postSubmitAction = action;
    }
  }

  isRecordExitPathCalled(exitPath: FeedbackAppExitPath): boolean {
    return this.exitPath !== null && this.exitPath === exitPath;
  }

  recordExitPath(exitPath: FeedbackAppExitPath|null) {
    if (this.exitPath === null) {
      this.exitPath = exitPath;
    }
  }

  getRecordPreSubmitActionCallCount(action: FeedbackAppPreSubmitAction):
      number {
    return this.preSubmitActionMap.get(action) || 0;
  }

  recordPreSubmitAction(action: FeedbackAppPreSubmitAction) {
    this.preSubmitActionMap.set(
        action, this.getRecordPreSubmitActionCallCount(action) + 1);
  }

  isHelpContentOutcomeMetricEmitted(outcome: FeedbackAppHelpContentOutcome):
      boolean {
    return this.helpContentOutcome !== null &&
        this.helpContentOutcome === outcome;
  }

  recordHelpContentOutcome(outcome: FeedbackAppHelpContentOutcome): void {
    assert(this.helpContentOutcome === null);
    this.helpContentOutcome = outcome;
  }
}
