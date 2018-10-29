// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Demo mode test utilities.
 */

/** Demo mode test helper. */
class DemoModeTestHelper {
  /**
   * Start demo mode setup for telemetry.
   * @param {string} demoConfig Identifies demo mode type.
   */
  static setUp(demoConfig) {
    chrome.send('startDemoModeSetupForTesting', [demoConfig]);
  }
}
