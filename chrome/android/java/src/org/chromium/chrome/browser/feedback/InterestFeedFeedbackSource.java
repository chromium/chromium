// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import org.chromium.chrome.browser.ChromeFeatureList;

import java.util.HashMap;
import java.util.Map;

/**
 * Provides information about whether the interest feed is enabled for use in feedback reports.
 */
public class InterestFeedFeedbackSource implements FeedbackSource {
    private static final String KEY = "Interest Feed";
    private static final String ENABLED_VALUE = "Enabled";
    private static final String DISABLED_VALUE = "Disabled";

    private final HashMap<String, String> mMap;

    InterestFeedFeedbackSource() {
        mMap = new HashMap<>(1);
        mMap.put(KEY,
                ChromeFeatureList.isEnabled(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
                        ? ENABLED_VALUE
                        : DISABLED_VALUE);
    }

    @Override
    public Map<String, String> getFeedback() {
        return mMap;
    }
}
