// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {string}
 * @const
 */
const FEEDBACK_LANDING_PAGE =
    'https://support.google.com/chrome/go/feedback_confirmation';

/** @type {string}
 * @const
 */
const FEEDBACK_LANDING_PAGE_TECHSTOP =
    'https://support.google.com/pixelbook/answer/7659411';

/** @type {string}
 * @const
 */
const FEEDBACK_LEGAL_HELP_URL =
    'https://support.google.com/legal/answer/3110420';

/** @type {string}
 * @const
 */
const FEEDBACK_PRIVACY_POLICY_URL = 'https://policies.google.com/privacy';

/** @type {string}
 * @const
 */
const FEEDBACK_TERM_OF_SERVICE_URL = 'https://policies.google.com/terms';

/**
 * Opens the supplied url in an app window. It uses the url as the window ID.
 * @param {string} url The destination URL for the link.
 */
function openUrlInAppWindow(url) {
  chrome.app.window.create(url, {
    frame: 'chrome',
    id: url,
    width: 640,
    height: 400,
    hidden: false,
    resizable: true
  });
}
