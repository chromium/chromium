// Copyright 2020 The Chromium Authors
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
import org.chromium.chrome.browser.feed.ScrollListener.ScrollState;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerState;

import java.util.ArrayList;
import java.util.List;

/** Unit test for {@link HeaderIphScrollListener}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
public final class HeaderIphScrollListenerTest {
    /** Parameter provider for testing the trigger of the IPH. */
    public static class TestParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            List<ParameterSet> parameters = new ArrayList<>();
            // Trigger IPH.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    true,
                                    ScrollState.IDLE,
                                    TriggerState.HAS_NOT_BEEN_DISPLAYED,
                                    10,
                                    true,
                                    true,
                                    true));
            // Don't trigger the IPH because the state is not set to has been displayed.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    false,
                                    ScrollState.IDLE,
                                    TriggerState.HAS_BEEN_DISPLAYED,
                                    10,
                                    true,
                                    true,
                                    true));
            // Don't trigger the IPH because there was not enough scroll done.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    false,
                                    ScrollState.IDLE,
                                    TriggerState.HAS_NOT_BEEN_DISPLAYED,
                                    1,
                                    true,
                                    true,
                                    true));
            // Don't trigger the IPH because the position in the stream is not suitable for the IPH.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    false,
                                    ScrollState.IDLE,
                                    TriggerState.HAS_NOT_BEEN_DISPLAYED,
                                    10,
                                    false,
                                    true,
                                    true));
            // Don't trigger the IPH because the feed is not expanded.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    false,
                                    ScrollState.IDLE,
                                    TriggerState.HAS_NOT_BEEN_DISPLAYED,
                                    10,
                                    false,
                                    false,
                                    true));
            // Don't trigger the IPH because the user is not signed in.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    false,
                                    ScrollState.IDLE,
                                    TriggerState.HAS_NOT_BEEN_DISPLAYED,
                                    10,
                                    false,
                                    true,
                                    false));
            return parameters;
        }
    }

    /** Parameter provider for testing the trigger of the IPH from scroll events. */
    public static class TestParamsForOnScroll extends TestParams {
        @Override
        public Iterable<ParameterSet> getParameters() {
            List<ParameterSet> parameters = new ArrayList<>();
            for (ParameterSet parameter : super.getParameters()) {
                parameters.add(parameter);
            }
            // Don't trigger the IPH because the scroll state is not IDLE.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    false,
                                    ScrollState.DRAGGING,
                                    TriggerState.HAS_NOT_BEEN_DISPLAYED,
                                    10,
                                    true,
                                    true,
                                    true));
            return parameters;
        }
    }

    /** Parameter provider for testing the trigger of the IPH from offset changes events. */
    public static class TestParamsForOnOffsetChanged extends TestParams {
        @Override
        public Iterable<ParameterSet> getParameters() {
            List<ParameterSet> parameters = new ArrayList<>();
            for (ParameterSet parameter : super.getParameters()) {
                parameters.add(parameter);
            }
            // Don't trigger the IPH because the vertical offset is 0.
            parameters.add(
                    new ParameterSet()
                            .value(
                                    false,
                                    ScrollState.IDLE,
                                    TriggerState.HAS_NOT_BEEN_DISPLAYED,
                                    0,
                                    true,
                                    true,
                                    true));
            return parameters;
        }
    }

    private static final int FEED_VIEW_HEIGHT = 100;

    @Mock private Tracker mTracker;

    private boolean mHasShownMenuIph;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @Feature({"Feed"})
    @ParameterAnnotations.UseMethodParameter(TestParamsForOnScroll.class)
    public void onScrollStateChanged_triggerIph(
            boolean expectEnabled,
            int scrollState,
            int triggerState,
            int verticalScrollOffset,
            boolean isFeedHeaderPositionInRecyclerViewSuitableForIPH,
            boolean isFeedExpanded,
            boolean isSignedIn) {
        // Set Tracker mock.
        when(mTracker.getTriggerState(FeatureConstants.FEED_HEADER_MENU_FEATURE))
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
                        return isFeedHeaderPositionInRecyclerViewSuitableForIPH;
                    }

                    @Override
                    public long getCurrentTimeMs() {
                        return 0;
                    }

                    @Override
                    public long getLastFetchTimeMs() {
                        return 0;
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
                    public void removeScrollListener(ScrollListener listener) {}

                    @Override
                    public int getVerticalScrollOffset() {
                        return verticalScrollOffset;
                    }

                    @Override
                    public int getRootViewHeight() {
                        return FEED_VIEW_HEIGHT;
                    }

                    @Override
                    public int getTopPositionRelativeToContainerView(View childView) {
                        return 0;
                    }
                };

        // Trigger IPH through the scroll listener.
        HeaderIphScrollListener listener =
                new HeaderIphScrollListener(
                        delegate,
                        scrollableContainerDelegate,
                        () -> {
                            mHasShownMenuIph = true;
                        });
        listener.onScrollStateChanged(scrollState);

        if (expectEnabled) {
            Assert.assertTrue(mHasShownMenuIph);
        } else {
            Assert.assertFalse(mHasShownMenuIph);
        }
    }

    @Test
    @Feature({"Feed"})
    @ParameterAnnotations.UseMethodParameter(TestParamsForOnOffsetChanged.class)
    public void onScrollStateChanged_onHeaderOffsetChanged(
            boolean expectEnabled,
            int scrollState,
            int triggerState,
            int verticalScrollOffset,
            boolean isFeedHeaderPositionInRecyclerViewSuitableForIPH,
            boolean isFeedExpanded,
            boolean isSignedIn) {
        // Set Tracker mock.
        when(mTracker.getTriggerState(FeatureConstants.FEED_HEADER_MENU_FEATURE))
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
                        return isFeedHeaderPositionInRecyclerViewSuitableForIPH;
                    }

                    @Override
                    public long getCurrentTimeMs() {
                        return 0;
                    }

                    @Override
                    public long getLastFetchTimeMs() {
                        return 0;
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
                    public void removeScrollListener(ScrollListener listener) {}

                    @Override
                    public int getVerticalScrollOffset() {
                        return 0;
                    }

                    @Override
                    public int getRootViewHeight() {
                        return FEED_VIEW_HEIGHT;
                    }

                    @Override
                    public int getTopPositionRelativeToContainerView(View childView) {
                        return 0;
                    }
                };

        // Trigger IPH through the scroll listener.
        HeaderIphScrollListener listener =
                new HeaderIphScrollListener(
                        delegate,
                        scrollableContainerDelegate,
                        () -> {
                            mHasShownMenuIph = true;
                        });
        listener.onHeaderOffsetChanged(-verticalScrollOffset);

        if (expectEnabled) {
            Assert.assertTrue(mHasShownMenuIph);
        } else {
            Assert.assertFalse(mHasShownMenuIph);
        }
    }
}
