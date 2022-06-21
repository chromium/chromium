// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Runtime setup before main.js is executed.

/**
 * @const {boolean}
 */
window.isSWA = true;

/**
 * Sets window.IN_TEST if this code is run in the test environment. We
 * detect this by checking for presence of domAutomationController.
 * @const {boolean}
 */
window.IN_TEST = window.IN_TEST || (() => {
                   return window.domAutomationController ? true : undefined;
                 })();
