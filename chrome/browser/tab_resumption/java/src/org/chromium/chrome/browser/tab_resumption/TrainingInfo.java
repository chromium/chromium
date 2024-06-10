// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.annotation.NonNull;

import org.chromium.components.visited_url_ranking.ScoredURLUserAction;

/**
 * Opaque IDs and helper functions to identify and record a suggestion to provide feedback for model
 * training. Per recordAction() implementation in C++, a suggestion can only be recorded once, and
 * then all subsequent attempts would be ignored. This behavior helps to simplify callers.
 */
public class TrainingInfo {
    private final long mNativeVisitedUrlRankingBackend;
    protected final String mVisitId;
    protected final long mRequestId;

    /**
     * @param nativeVisitedUrlRankingBackend Required to access recordAction() via JNI. For tests
     *     this should be set to 0L.
     * @param visitId Opaque ID to identify the suggestion. Non-empty.
     * @param requestId Opaque ID to identify the training request. Non-negative.
     */
    TrainingInfo(long nativeVisitedUrlRankingBackend, @NonNull String visitId, long requestId) {
        mNativeVisitedUrlRankingBackend = nativeVisitedUrlRankingBackend;
        assert !visitId.isEmpty();
        // No restrictions on `requestId`.

        mVisitId = visitId;
        mRequestId = requestId;
    }

    /** Records user action to provide feedback for suggestion model training. */
    void record(@ScoredURLUserAction int scoredUrlUserAction) {
        VisitedUrlRankingBackendJni.get()
                .recordAction(
                        mNativeVisitedUrlRankingBackend, scoredUrlUserAction, mVisitId, mRequestId);
    }
}
