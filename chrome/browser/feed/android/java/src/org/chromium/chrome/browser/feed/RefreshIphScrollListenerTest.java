// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.params.BlockJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerState;

import java.util.ArrayList;
import java.util.List;

/** Unit test for {@link RefreshIphScrollListener}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
public final class RefreshIphScrollListenerTest {
    /** Parameter provider for testing the trigger of the IPH. */
    public static class TestParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            List<ParameterSet> parameters = new ArrayList<>();
            // Trigger the IPH when the user is signed in.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    true,
                                    10,
                                    TriggerState.HAS_NOT_BEEN_DISPLAYED,
                                    true,
                                    true,
                                    100,
                                    100 + RefreshIphScrollListener.FETCH_TIME_AGE_THREASHOLD_MS,
                                    false));
            // Trigger the IPH when the user is not signed in.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    true,
                                    10,
                                    TriggerState.HAS_NOT_BEEN_DISPLAYED,
                                    true,
                                    false,
                                    100,
                                    100 + RefreshIphScrollListener.FETCH_TIME_AGE_THREASHOLD_MS,
                                    false));
            // Don't trigger the IPH because the state is not set to has been displayed.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    false,
                                    10,
                                    TriggerState.HAS_BEEN_DISPLAYED,
                                    true,
                                    true,
                                    100,
                                    100 + RefreshIphScrollListener.FETCH_TIME_AGE_THREASHOLD_MS,
                                    false));
            // Don't trigger the IPH because the feed is not expanded.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    false,
                                    10,
                                    TriggerState.HAS_NOT_BEEN_DISPLAYED,
                                    false,
                                    true,
                                    100,
                                    100 + RefreshIphScrollListener.FETCH_TIME_AGE_THREASHOLD_MS,
                                    false));
            // Don't trigger the IPH because the scrollY is 0.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    false,
                                    0,
                                    TriggerState.HAS_NOT_BEEN_DISPLAYED,
                                    true,
                                    true,
                                    100,
                                    100 + RefreshIphScrollListener.FETCH_TIME_AGE_THREASHOLD_MS,
                                    false));
            // Don't trigger the IPH because the last fetch time is not available.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    false,
                                    10,
                                    TriggerState.HAS_NOT_BEEN_DISPLAYED,
                                    true,
                                    true,
                                    0,
                                    100 + RefreshIphScrollListener.FETCH_TIME_AGE_THREASHOLD_MS + 1,
                                    false));
            // Don't trigger the IPH because the last fetch time is still within the threshold.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    false,
                                    10,
                                    TriggerState.HAS_NOT_BEEN_DISPLAYED,
                                    true,
                                    true,
                                    100,
                                    99 + RefreshIphScrollListener.FETCH_TIME_AGE_THREASHOLD_MS,
                                    false));
            // Don't trigger the IPH because the page can still scroll up.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    false,
                                    0,
                                    TriggerState.HAS_NOT_BEEN_DISPLAYED,
                                    true,
                                    true,
                                    100,
                                    100 + RefreshIphScrollListener.FETCH_TIME_AGE_THREASHOLD_MS,
                                    true));
            return parameters;
        }
    }

    @Mock private Tracker mTracker;

    private boolean mHasShownIPH;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @Feature({"Feed"})
    @ParameterAnnotations.UseMethodParameter(TestParams.class)
    public void triggerIph(
            boolean expectEnabled,
            int scrollY,
            int triggerState,
            boolean isFeedExpanded,
            boolean isSignedIn,
            long lastFetchTimeMs,
            long currentTimeMs,
            boolean canScrollUp) {
        // Set Tracker mock.
        when(mTracker.getTriggerState(FeatureConstants.FEED_SWIPE_REFRESH_FEATURE))
                .thenReturn(triggerState);

        FeedBubbleDelegate delegate =
                new FeedBubbleDelegate() {
                    @Override
                    public Tracker getFeatureEngagementTracker() {
                        return mTracker;
                    }

                    @Override
                    public boolean isFeedExpanded() {
                        return isFeedExpanded;
                    }

                    @Override
                    public boolean isSignedIn() {
                        return isSignedIn;
                    }

                    @Override
                    public boolean isFeedHeaderPositionInContainerSuitableForIPH(
                            float headerMaxPosFraction) {
                        return false;
                    }

                    @Override
                    public long getCurrentTimeMs() {
                        return currentTimeMs;
                    }

                    @Override
                    public long getLastFetchTimeMs() {
                        return lastFetchTimeMs;
                    }

                    @Override
                    public boolean canScrollUp() {
                        return canScrollUp;
                    }
                };

        ScrollableContainerDelegate scrollableContainerDelegate =
                new ScrollableContainerDelegate() {
                    @Override
                    public void addScrollListener(ScrollListener listener) {}

                    @Override
                    public void removeScrollListener(ScrollListener listener) {}

                    @Override
                    public int getVerticalScrollOffset() {
                        return 10;
                    }

                    @Override
                    public int getRootViewHeight() {
                        return 100;
                    }

                    @Override
                    public int getTopPositionRelativeToContainerView(View childView) {
                        return 0;
                    }
                };

        // Trigger IPH through the scroll listener.
        RefreshIphScrollListener listener =
                new RefreshIphScrollListener(
                        delegate,
                        scrollableContainerDelegate,
                        () -> {
                            mHasShownIPH = true;
                        });
        listener.onScrolled(0, scrollY);

        if (expectEnabled) {
            Assert.assertTrue(mHasShownIPH);
        } else {
            Assert.assertFalse(mHasShownIPH);
        }
    }
}
