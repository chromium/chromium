// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.app_rating;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import com.google.android.gms.tasks.Task;
import com.google.android.play.core.review.ReviewException;
import com.google.android.play.core.review.ReviewInfo;
import com.google.android.play.core.review.ReviewManager;
import com.google.android.play.core.review.ReviewManagerFactory;
import com.google.android.play.core.review.model.ReviewErrorCode;

import org.chromium.base.ContextUtils;
import org.chromium.base.TimeUtils;
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
                        long startTime = TimeUtils.elapsedRealtimeMillis();
                        Task<Void> flow = mReviewManager.launchReviewFlow(activity, reviewInfo);
                        flow.addOnCompleteListener(
                                flowTask -> {
                                    long duration = TimeUtils.elapsedRealtimeMillis() - startTime;
                                    AppRatingPromoMetrics.recordInteractionDuration(duration);
                                    AppRatingPromoMetrics.recordReviewStatus(
                                            ReviewErrorCode.NO_ERROR);
                                    onComplete.run();
                                });
                    } else {
                        Exception exception = task.getException();
                        int errorCode = Integer.MIN_VALUE; // Default unknown error
                        if (exception instanceof ReviewException) {
                            @ReviewErrorCode
                            int reviewErrorCode = ((ReviewException) exception).getErrorCode();
                            errorCode = reviewErrorCode;
                        }
                        AppRatingPromoMetrics.recordReviewStatus(errorCode);
                        onComplete.run();
                    }
                });
    }
}
