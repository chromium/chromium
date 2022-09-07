// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Provides interfaces for emitting metrics from demo mode apps to UMA.
 */
class DemoMetricsService {
  constructor() {}

  // Record the action that the user breaks the current Attract Loop.
  recordAttractLoopBreak() {
    chrome.metricsPrivateIndividualApis.recordUserAction(
        'DemoMode.AttractLoop.Break');
  }
}

export const demoMetricsService = new DemoMetricsService();