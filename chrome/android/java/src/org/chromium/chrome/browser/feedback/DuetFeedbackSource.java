// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import org.chromium.chrome.browser.flags.FeatureUtilities;

import java.util.HashMap;
import java.util.Map;

/** Provides whether Duet is enabled for feedback reports. */
public class DuetFeedbackSource implements FeedbackSource {
    private static final String DUET_KEY = "Duet";
    private static final String ENABLED_VALUE = "Enabled";
    private static final String DISABLED_VALUE = "Disabled";

    private final HashMap<String, String> mMap;

    DuetFeedbackSource() {
        mMap = new HashMap<>(1);
        mMap.put(DUET_KEY,
                FeatureUtilities.isBottomToolbarEnabled() ? ENABLED_VALUE : DISABLED_VALUE);
    }

    @Override
    public Map<String, String> getFeedback() {
        return mMap;
    }
}
