// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const FEEDBACK_LANDING_PAGE: string =
    'https://support.google.com/chrome/go/feedback_confirmation';

export const FEEDBACK_LANDING_PAGE_TECHSTOP: string =
    'https://support.google.com/pixelbook/answer/7659411';

export const FEEDBACK_LEGAL_HELP_URL: string =
    'https://support.google.com/legal/answer/3110420';

export const FEEDBACK_PRIVACY_POLICY_URL: string =
    'https://policies.google.com/privacy';

export const FEEDBACK_TERM_OF_SERVICE_URL: string =
    'https://policies.google.com/terms';

/**
 * Opens the supplied url in an app window. It uses the url as the window ID.
 * @param url The destination URL for the link.
 */
export function openUrlInAppWindow(url: string) {
  const params = `status=no,location=no,toolbar=no,menubar=no,
  width=640,height=400,left=200,top=200`;

  window.open(url, url, params);
}
