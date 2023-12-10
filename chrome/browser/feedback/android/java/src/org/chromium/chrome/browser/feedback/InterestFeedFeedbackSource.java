// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import androidx.annotation.Nullable;

import java.util.HashMap;
import java.util.Map;

/** Provides information about whether the interest feed is enabled for use in feedback reports. */
public class InterestFeedFeedbackSource implements FeedbackSource {
    private static final String KEY = "Interest Feed";
    private static final String ENABLED_VALUE = "Enabled";

    private final HashMap<String, String> mMap;

    InterestFeedFeedbackSource(@Nullable Map<String, String> feedContext) {
        mMap = new HashMap<>();
        mMap.put(KEY, ENABLED_VALUE);

        // For each key in feedContext, add that to the feedback map too.
        if (feedContext != null) mMap.putAll(feedContext);
    }

    @Override
    public Map<String, String> getFeedback() {
        return mMap;
    }
}
