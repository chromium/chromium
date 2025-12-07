// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feedback.ConnectivityTask.FeedbackData;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Map;

/** Grabs feedback about the current phone connectivity status. */
@NullMarked
class ConnectivityFeedbackSource
        implements AsyncFeedbackSource, ConnectivityTask.ConnectivityResult {
    /** The timeout (ms) for gathering connection data. */
    private static final int CONNECTIVITY_CHECK_TIMEOUT_MS = 5000;

    private final Profile mProfile;

    private @Nullable Runnable mCallback;
    private @Nullable ConnectivityTask mConnectivityTask;

    ConnectivityFeedbackSource(Profile profile) {
        mProfile = profile;
    }

    // AsyncFeedbackSource implementation.
    @Override
    public @Nullable Map<String, String> getFeedback() {
        if (mConnectivityTask == null) return null;
        return mConnectivityTask.get().toMap();
    }

    @Override
    public void start(Runnable callback) {
        mCallback = callback;
        mConnectivityTask = ConnectivityTask.create(mProfile, CONNECTIVITY_CHECK_TIMEOUT_MS, this);
    }

    @Override
    public boolean isReady() {
        return assumeNonNull(mConnectivityTask).isDone();
    }

    // ConnectivityTask.ConnectivityResult implementation.
    @Override
    public void onResult(FeedbackData feedbackData) {
        assumeNonNull(mCallback).run();
    }
}
