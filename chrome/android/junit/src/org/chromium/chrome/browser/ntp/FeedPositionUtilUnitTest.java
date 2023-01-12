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
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
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
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.segmentation_platform.SegmentSelectionResult;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.components.segmentation_platform.proto.SegmentationProto.SegmentId;

/** Unit tests for {@link FeedPositionUtils} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedPositionUtilUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private final TestValues mTestValues = new TestValues();

    @Mock
    Profile mProfile;
    @Mock
    SegmentationPlatformService mSegmentationPlatformService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Profile.setLastUsedProfileForTesting(mProfile);
        SegmentationPlatformServiceFactory.setForTests(mSegmentationPlatformService);
        setSegmentationResult(new SegmentSelectionResult(false, null));
    }

    @After
    public void tearDown() {
        FeatureList.setTestValues(null);
    }

    private void setSegmentationResult(SegmentSelectionResult segmentSelectionResult) {
        Mockito.doAnswer(invocation -> {
                   Callback<SegmentSelectionResult> callback = invocation.getArgument(1);
                   callback.onResult(segmentSelectionResult);
                   return null;
               })
                .when(mSegmentationPlatformService)
                .getSelectedSegment(eq(FEED_USER_SEGMENT_KEY), any());

        FeedPositionUtils.cacheSegmentationResult();
    }

    @Test
    @SmallTest
    public void testIsFeedPushDownSmallEnabled() {
        setFeedPositionFlags(FeedPositionUtils.PUSH_DOWN_FEED_SMALL, "");
        Assert.assertTrue(FeedPositionUtils.isFeedPushDownSmallEnabled());

        setFeedPositionFlags(FeedPositionUtils.PUSH_DOWN_FEED_SMALL, "active");
        Assert.assertFalse(FeedPositionUtils.isFeedPushDownSmallEnabled());

        setSegmentationResult(new SegmentSelectionResult(
                true, SegmentId.OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER));
        Assert.assertTrue(FeedPositionUtils.isFeedPushDownSmallEnabled());
    }

    @Test
    @SmallTest
    public void testIsFeedPushDownLargeEnabled() {
        setFeedPositionFlags(FeedPositionUtils.PUSH_DOWN_FEED_LARGE, "");
        Assert.assertTrue(FeedPositionUtils.isFeedPushDownLargeEnabled());

        setFeedPositionFlags(FeedPositionUtils.PUSH_DOWN_FEED_SMALL, "active");
        Assert.assertFalse(FeedPositionUtils.isFeedPushDownLargeEnabled());

        setSegmentationResult(new SegmentSelectionResult(
                true, SegmentId.OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER));
        Assert.assertTrue(FeedPositionUtils.isFeedPushDownLargeEnabled());
    }

    @Test
    @SmallTest
    public void testIsFeedPullUpEnabled() {
        setFeedPositionFlags(FeedPositionUtils.PULL_UP_FEED, "");
        Assert.assertTrue(FeedPositionUtils.isFeedPullUpEnabled());

        setFeedPositionFlags(FeedPositionUtils.PULL_UP_FEED, "non-active");
        Assert.assertFalse(FeedPositionUtils.isFeedPullUpEnabled());

        setSegmentationResult(new SegmentSelectionResult(
                true, SegmentId.OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER));
        Assert.assertFalse(FeedPositionUtils.isFeedPullUpEnabled());

        setSegmentationResult(
                new SegmentSelectionResult(true, SegmentId.OPTIMIZATION_TARGET_UNKNOWN));
        Assert.assertTrue(FeedPositionUtils.isFeedPullUpEnabled());
    }

    private void setFeedPositionFlags(
            String feedPositionVariation, String targetFeedOrNonFeedUsersValue) {
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.FEED_POSITION_ANDROID, feedPositionVariation, "true");
        mTestValues.addFieldTrialParamOverride(ChromeFeatureList.FEED_POSITION_ANDROID,
                FeedPositionUtils.FEED_ACTIVE_TARGETING, targetFeedOrNonFeedUsersValue);
        FeatureList.setTestValues(mTestValues);

        Assert.assertEquals(targetFeedOrNonFeedUsersValue,
                FeedPositionUtils.getTargetFeedOrNonFeedUsersParam());
    }
}
