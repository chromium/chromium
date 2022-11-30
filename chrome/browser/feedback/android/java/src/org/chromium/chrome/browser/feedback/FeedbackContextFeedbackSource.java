// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.chromium.base.CollectionUtil;

import java.util.Map;

/**
 * Provides information about which feature was being used when the feedback report was triggered.
 */
public class FeedbackContextFeedbackSource implements FeedbackSource {
    static final String FEEDBACK_CONTEXT_KEY = "Feedback Context";
    private final String mFeedbackContext;

    FeedbackContextFeedbackSource(@Nullable String feedbackContext) {
        mFeedbackContext = feedbackContext;
    }

    @Override
    public Map<String, String> getFeedback() {
        if (TextUtils.isEmpty(mFeedbackContext)) return null;
        return CollectionUtil.newHashMap(Pair.create(FEEDBACK_CONTEXT_KEY, mFeedbackContext));
    }
}
