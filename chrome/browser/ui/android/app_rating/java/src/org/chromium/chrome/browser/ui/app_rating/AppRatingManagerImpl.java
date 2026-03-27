// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.app_rating;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import com.google.android.gms.tasks.Task;
import com.google.android.play.core.review.ReviewInfo;
import com.google.android.play.core.review.ReviewManager;
import com.google.android.play.core.review.ReviewManagerFactory;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.ServiceImpl;

/** Implementation of AppRatingManager using the Google Play Store In-App Review API. */
@NullMarked
@ServiceImpl(AppRatingManager.class)
public class AppRatingManagerImpl implements AppRatingManager {
    private final ReviewManager mReviewManager;

    public AppRatingManagerImpl() {
        mReviewManager = ReviewManagerFactory.create(ContextUtils.getApplicationContext());
    }

    /**
     * Constructor for testing.
     *
     * @param reviewManager The ReviewManager to use.
     */
    @VisibleForTesting
    AppRatingManagerImpl(ReviewManager reviewManager) {
        mReviewManager = reviewManager;
    }

    @Override
    public void requestAndShowReviewFlow(Activity activity, Runnable onComplete) {
        Task<ReviewInfo> request = mReviewManager.requestReviewFlow();
        request.addOnCompleteListener(
                task -> {
                    if (task.isSuccessful()) {
                        ReviewInfo reviewInfo = task.getResult();
                        Task<Void> flow = mReviewManager.launchReviewFlow(activity, reviewInfo);
                        flow.addOnCompleteListener(flowTask -> onComplete.run());
                    } else {
                        onComplete.run();
                    }
                });
    }
}
