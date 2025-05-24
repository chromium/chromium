// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Map;

/**
 * Provides information about which feature was being used when the feedback report was triggered.
 */
@NullMarked
public class FeedbackContextFeedbackSource implements FeedbackSource {
    static final String FEEDBACK_CONTEXT_KEY = "Feedback Context";
    private final @Nullable String mFeedbackContext;

    FeedbackContextFeedbackSource(@Nullable String feedbackContext) {
        mFeedbackContext = feedbackContext;
    }

    @Override
    public @Nullable Map<String, String> getFeedback() {
        if (TextUtils.isEmpty(mFeedbackContext)) return null;
        return Map.of(FEEDBACK_CONTEXT_KEY, mFeedbackContext);
    }
}
