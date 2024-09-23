// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.SegmentationPlatformServiceFactory;
import org.chromium.components.segmentation_platform.PredictionOptions;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.components.segmentation_platform.prediction_status.PredictionStatus;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A class to handle the state of flags for Feed position experiment. */
public class FeedPositionUtils {
    // The key is used to decide whether the user likes to use Feed. Should be consistent with
    // |kFeedUserSegmentationKey| in config.h in components/segmentation_platform/.
    public static final String FEED_USER_SEGMENT_KEY = "feed_user_segment";
    public static final String FEED_ACTIVE_TARGETING = "feed_active_targeting";
    public static final String PULL_UP_FEED = "pull_up_feed";

    // Constants with FeedPositionSegmentationResult in enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        FeedPositionSegmentationResult.UNINITIALIZED,
        FeedPositionSegmentationResult.IS_FEED_ACTIVE_USER,
        FeedPositionSegmentationResult.IS_NON_FEED_ACTIVE_USER,
        FeedPositionSegmentationResult.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FeedPositionSegmentationResult {
        int UNINITIALIZED = 0;
        int IS_FEED_ACTIVE_USER = 1;
        int IS_NON_FEED_ACTIVE_USER = 2;
        int NUM_ENTRIES = 3;
    }

    /** Returns whether the pulling up Feed experiment is enabled. */
    public static boolean isFeedPullUpEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.FEED_POSITION_ANDROID, PULL_UP_FEED, false)
                && getBehaviourResultFromSegmentation();
    }

    /**
     * Returns the string for whether we should target to Feed active users or non-Feed users:
     * 1. "active" means targeting to Feed active users.
     * 2. "non-active" means targeting to Non-Feed active users.
     * 3. Other string (including empty string) is for no targeting.
     */
    @VisibleForTesting
    static String getTargetFeedOrNonFeedUsersParam() {
        return ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.FEED_POSITION_ANDROID, FEED_ACTIVE_TARGETING);
    }

    /**
     * Called to check whether feed position should be enabled based on segmentation model result.
     * Check whether we should target Feed active users, if not, return true; if so, check whether
     * the user is a Feed/non-Feed active users accordingly.
     */
    private static boolean getBehaviourResultFromSegmentation() {
        String targetFeedOrNonFeedUsersParam = getTargetFeedOrNonFeedUsersParam();
        if (targetFeedOrNonFeedUsersParam == null) return true;

        @FeedPositionSegmentationResult int resultEnum = FeedPositionUtils.getSegmentationResult();
        RecordHistogram.recordEnumeratedHistogram(
                "NewTabPage.FeedPositionSegmentationResult",
                resultEnum,
                FeedPositionSegmentationResult.NUM_ENTRIES);
        switch (targetFeedOrNonFeedUsersParam) {
            case "active":
                return resultEnum == FeedPositionSegmentationResult.IS_FEED_ACTIVE_USER;
            case "non-active":
                return resultEnum == FeedPositionSegmentationResult.IS_NON_FEED_ACTIVE_USER;
        }
        return true;
    }

    /**
     * Called to get segment selection result from shared prefs.
     * @return The segmentation result.
     */
    public static @FeedPositionSegmentationResult int getSegmentationResult() {
        return ChromeSharedPreferences.getInstance()
                .readInt(
                        ChromePreferenceKeys.SEGMENTATION_FEED_ACTIVE_USER,
                        FeedPositionSegmentationResult.IS_NON_FEED_ACTIVE_USER);
    }

    /**
     * Called to cache segment selection result from segmentation platform service into prefs for
     * synchronous API calls. Called early during initialization.
     */
    public static void cacheSegmentationResult(Profile profile) {
        SegmentationPlatformService segmentationPlatformService =
                SegmentationPlatformServiceFactory.getForProfile(profile);
        PredictionOptions options = new PredictionOptions(/* onDemandExecution= */ false);
        segmentationPlatformService.getClassificationResult(
                FEED_USER_SEGMENT_KEY,
                options,
                null,
                result -> {
                    @FeedPositionSegmentationResult int resultEnum;
                    if (result.status != PredictionStatus.SUCCEEDED
                            || result.orderedLabels.isEmpty()) {
                        resultEnum = FeedPositionSegmentationResult.UNINITIALIZED;
                    } else if (result.orderedLabels.get(0).equals("FeedUser")) {
                        resultEnum = FeedPositionSegmentationResult.IS_FEED_ACTIVE_USER;
                    } else {
                        resultEnum = FeedPositionSegmentationResult.IS_NON_FEED_ACTIVE_USER;
                    }
                    ChromeSharedPreferences.getInstance()
                            .writeInt(
                                    ChromePreferenceKeys.SEGMENTATION_FEED_ACTIVE_USER, resultEnum);
                });
    }
}
