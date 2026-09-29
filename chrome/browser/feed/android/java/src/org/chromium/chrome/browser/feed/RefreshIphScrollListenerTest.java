// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.params.BlockJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

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
                                    true,
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
                                    true,
                                    true,
                                    false,
                                    100,
                                    100 + RefreshIphScrollListener.FETCH_TIME_AGE_THREASHOLD_MS,
                                    false));
            // Don't trigger the IPH because wouldTriggerHelpUi returns false.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    false,
                                    10,
                                    false,
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
                                    true,
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
                                    true,
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
                                    true,
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
                                    true,
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
                                    true,
                                    true,
                                    true,
                                    100,
                                    100 + RefreshIphScrollListener.FETCH_TIME_AGE_THREASHOLD_MS,
                                    true));
            return parameters;
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Tracker mTracker;

    private boolean mHasShownIph;

    @Test
    @Feature({"Feed"})
    @ParameterAnnotations.UseMethodParameter(TestParams.class)
    public void triggerIph(
            boolean expectEnabled,
            int scrollY,
            boolean wouldTriggerHelpUi,
            boolean isFeedExpanded,
            boolean isSignedIn,
            long lastFetchTimeMs,
            long currentTimeMs,
            boolean canScrollUp) {
        // Set Tracker mock.
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.FEED_SWIPE_REFRESH_FEATURE))
                .thenReturn(wouldTriggerHelpUi);

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
                    public boolean isFeedHeaderPositionInContainerSuitableForIph(
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
                            mHasShownIph = true;
                        });
        listener.onScrolled(0, scrollY);

        if (expectEnabled) {
            Assert.assertTrue(mHasShownIph);
        } else {
            Assert.assertFalse(mHasShownIph);
        }
    }

    @Test
    @Feature({"Feed"})
    public void testNotInitialized_doesNotTriggerNorRemoveListener() {
        when(mTracker.isInitialized()).thenReturn(false);
        boolean[] listenerRemoved = new boolean[1];

        FeedBubbleDelegate delegate =
                new FeedBubbleDelegate() {
                    @Override
                    public Tracker getFeatureEngagementTracker() {
                        return mTracker;
                    }

                    @Override
                    public boolean isFeedExpanded() {
                        return true;
                    }

                    @Override
                    public boolean isSignedIn() {
                        return true;
                    }

                    @Override
                    public boolean isFeedHeaderPositionInContainerSuitableForIph(
                            float headerMaxPosFraction) {
                        return false;
                    }

                    @Override
                    public long getCurrentTimeMs() {
                        return 100 + RefreshIphScrollListener.FETCH_TIME_AGE_THREASHOLD_MS;
                    }

                    @Override
                    public long getLastFetchTimeMs() {
                        return 100;
                    }

                    @Override
                    public boolean canScrollUp() {
                        return false;
                    }
                };

        ScrollableContainerDelegate scrollableContainerDelegate =
                new ScrollableContainerDelegate() {
                    @Override
                    public void addScrollListener(ScrollListener listener) {}

                    @Override
                    public void removeScrollListener(ScrollListener listener) {
                        listenerRemoved[0] = true;
                    }

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

        RefreshIphScrollListener listener =
                new RefreshIphScrollListener(
                        delegate,
                        scrollableContainerDelegate,
                        () -> {
                            mHasShownIph = true;
                        });
        listener.onScrolled(0, 10);

        Assert.assertFalse(mHasShownIph);
        Assert.assertFalse(listenerRemoved[0]);
    }

    @Test
    @Feature({"Feed"})
    public void testAlreadyTriggered_removesListener() {
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.hasEverTriggered(FeatureConstants.FEED_SWIPE_REFRESH_FEATURE, true))
                .thenReturn(true);
        boolean[] listenerRemoved = new boolean[1];

        FeedBubbleDelegate delegate =
                new FeedBubbleDelegate() {
                    @Override
                    public Tracker getFeatureEngagementTracker() {
                        return mTracker;
                    }

                    @Override
                    public boolean isFeedExpanded() {
                        return true;
                    }

                    @Override
                    public boolean isSignedIn() {
                        return true;
                    }

                    @Override
                    public boolean isFeedHeaderPositionInContainerSuitableForIph(
                            float headerMaxPosFraction) {
                        return false;
                    }

                    @Override
                    public long getCurrentTimeMs() {
                        return 100 + RefreshIphScrollListener.FETCH_TIME_AGE_THREASHOLD_MS;
                    }

                    @Override
                    public long getLastFetchTimeMs() {
                        return 100;
                    }

                    @Override
                    public boolean canScrollUp() {
                        return false;
                    }
                };

        ScrollableContainerDelegate scrollableContainerDelegate =
                new ScrollableContainerDelegate() {
                    @Override
                    public void addScrollListener(ScrollListener listener) {}

                    @Override
                    public void removeScrollListener(ScrollListener listener) {
                        listenerRemoved[0] = true;
                    }

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

        RefreshIphScrollListener listener =
                new RefreshIphScrollListener(
                        delegate,
                        scrollableContainerDelegate,
                        () -> {
                            mHasShownIph = true;
                        });
        listener.onScrolled(0, 10);

        Assert.assertFalse(mHasShownIph);
        Assert.assertTrue(listenerRemoved[0]);
    }
}
