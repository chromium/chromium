// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;

import static org.chromium.chrome.browser.ntp.FeedPositionUtils.FEED_USER_SEGMENT_KEY;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.SegmentationPlatformServiceFactory;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.components.segmentation_platform.prediction_status.PredictionStatus;

/** Unit tests for {@link FeedPositionUtils} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedPositionUtilUnitTest {

    private final TestValues mTestValues = new TestValues();

    @Mock Profile mProfile;
    @Mock SegmentationPlatformService mSegmentationPlatformService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        SegmentationPlatformServiceFactory.setForTests(mSegmentationPlatformService);
        setClassificationResult(new ClassificationResult(PredictionStatus.NOT_READY, null));
    }

    @After
    public void tearDown() {
        FeatureList.setTestValues(null);
    }

    private void setClassificationResult(ClassificationResult classificationResult) {
        Mockito.doAnswer(
                        invocation -> {
                            Callback<ClassificationResult> callback = invocation.getArgument(3);
                            callback.onResult(classificationResult);
                            return null;
                        })
                .when(mSegmentationPlatformService)
                .getClassificationResult(eq(FEED_USER_SEGMENT_KEY), any(), any(), any());

        FeedPositionUtils.cacheSegmentationResult(mProfile);
    }

    @Test
    @SmallTest
    public void testIsFeedPullUpEnabled() {
        setFeedPositionFlags(FeedPositionUtils.PULL_UP_FEED, "");
        Assert.assertTrue(FeedPositionUtils.isFeedPullUpEnabled());

        setFeedPositionFlags(FeedPositionUtils.PULL_UP_FEED, "non-active");
        Assert.assertFalse(FeedPositionUtils.isFeedPullUpEnabled());

        setClassificationResult(
                new ClassificationResult(PredictionStatus.SUCCEEDED, new String[] {"FeedUser"}));
        Assert.assertFalse(FeedPositionUtils.isFeedPullUpEnabled());

        setClassificationResult(
                new ClassificationResult(PredictionStatus.SUCCEEDED, new String[] {"Other"}));
        Assert.assertTrue(FeedPositionUtils.isFeedPullUpEnabled());
    }

    private void setFeedPositionFlags(
            String feedPositionVariation, String targetFeedOrNonFeedUsersValue) {
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.FEED_POSITION_ANDROID, feedPositionVariation, "true");
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.FEED_POSITION_ANDROID,
                FeedPositionUtils.FEED_ACTIVE_TARGETING,
                targetFeedOrNonFeedUsersValue);
        FeatureList.setTestValues(mTestValues);

        Assert.assertEquals(
                targetFeedOrNonFeedUsersValue,
                FeedPositionUtils.getTargetFeedOrNonFeedUsersParam());
    }
}
