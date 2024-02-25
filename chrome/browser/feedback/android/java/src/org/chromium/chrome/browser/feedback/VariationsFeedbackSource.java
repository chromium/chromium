// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.variations.VariationsAssociatedData;

import java.util.Map;

/** Grabs feedback about the current variations state. */
class VariationsFeedbackSource implements FeedbackSource {
    private final boolean mIsOffTheRecord;

    VariationsFeedbackSource(Profile profile) {
        mIsOffTheRecord = profile.isOffTheRecord();
    }

    @Override
    public Map<String, String> getFeedback() {
        if (mIsOffTheRecord) return null;
        return VariationsAssociatedData.getFeedbackMap();
    }
}
