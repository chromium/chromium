// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import org.chromium.base.metrics.RecordUserAction;

/**
 * A delegate class to record metrics associated with each card inside
 * Privacy Guide {@link PrivacyGuideFragment}.
 */
public class PrivacyGuideMetricsDelegate {
    /**
     * A method to record metrics on the next click of the privacy guide welcome page.
     */
    static void recordMetricsForWelcomeCard() {
        RecordUserAction.record("Settings.PrivacyGuide.NextClickWelcome");
    }
}
