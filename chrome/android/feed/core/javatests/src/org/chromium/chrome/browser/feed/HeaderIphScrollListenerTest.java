// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.mockito.Mockito.when;

import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ntp.ScrollListener;
import org.chromium.chrome.browser.ntp.ScrollListener.ScrollState;
import org.chromium.chrome.browser.ntp.ScrollableContainerDelegate;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerState;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link HeaderIphScrollListener}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public final class HeaderIphScrollListenerTest {
    /** Parameter provider for testing the trigger of the IPH. */
    public static class TestParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            List<ParameterSet> parameters = new ArrayList<>();
            // Trigger IPH.
            parameters.add(new ParameterSet().value(true, ScrollState.IDLE,
                    TriggerState.HAS_NOT_BEEN_DISPLAYED, true, 10, true, true, true));
            // Don't trigger the IPH because the state is not set to has been displayed.
            parameters.add(new ParameterSet().value(false, ScrollState.IDLE,
                    TriggerState.HAS_BEEN_DISPLAYED, true, 10, true, true, true));
            // Don't trigger the IPH because because the tracker would not trigger.
            parameters.add(new ParameterSet().value(false, ScrollState.IDLE,
                    TriggerState.HAS_NOT_BEEN_DISPLAYED, false, 10, true, true, true));
            // Don't trigger the IPH because there was not enough scroll done.
            parameters.add(new ParameterSet().value(false, ScrollState.IDLE,
                    TriggerState.HAS_NOT_BEEN_DISPLAYED, true, 1, true, true, true));
            // Don't trigger the IPH because the position in the stream is not suitable for the IPH.
            parameters.add(new ParameterSet().value(false, ScrollState.IDLE,
                    TriggerState.HAS_NOT_BEEN_DISPLAYED, true, 10, false, true, true));
            // Don't trigger the IPH because the feed is not expanded.
            parameters.add(new ParameterSet().value(false, ScrollState.IDLE,
                    TriggerState.HAS_NOT_BEEN_DISPLAYED, true, 10, false, false, true));
            // Don't trigger the IPH because the user is not signed in.
            parameters.add(new ParameterSet().value(false, ScrollState.IDLE,
                    TriggerState.HAS_NOT_BEEN_DISPLAYED, true, 10, false, true, false));
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
            parameters.add(new ParameterSet().value(false, ScrollState.DRAGGING,
                    TriggerState.HAS_NOT_BEEN_DISPLAYED, true, 10, true, true, true));
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
            parameters.add(new ParameterSet().value(false, ScrollState.IDLE,
                    TriggerState.HAS_NOT_BEEN_DISPLAYED, true, 0, true, true, true));
            return parameters;
        }
    }

    private static final int FEED_VIEW_HEIGHT = 100;

    @Mock
    private Tracker mTracker;
    private View mFeedRootView;

    private boolean mHasShownMenuIph;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mFeedRootView = new View(InstrumentationRegistry.getContext());
        mFeedRootView.layout(0, 0, 0, FEED_VIEW_HEIGHT);
    }

    @Test
    @MediumTest
    @Feature({"Feed"})
    @ParameterAnnotations.UseMethodParameter(TestParamsForOnScroll.class)
    public void onScrollStateChanged_triggerIph(boolean expectEnabled, int scrollState,
            int triggerState, boolean wouldTriggerHelpUI, int verticalScrollOffset,
            boolean isFeedHeaderPositionInRecyclerViewSuitableForIPH, boolean isFeedExpanded,
            boolean isSignedIn) {
        // Set Tracker mock.
        when(mTracker.getTriggerState(FeatureConstants.FEED_HEADER_MENU_FEATURE))
                .thenReturn(triggerState);
        when(mTracker.wouldTriggerHelpUI(FeatureConstants.FEED_HEADER_MENU_FEATURE))
                .thenReturn(wouldTriggerHelpUI);

        HeaderIphScrollListener.Delegate iphDelegate = new HeaderIphScrollListener.Delegate() {
            @Override
            public Tracker getFeatureEngagementTracker() {
                return mTracker;
            }
            @Override
            public void showMenuIph() {
                mHasShownMenuIph = true;
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
                new HeaderIphScrollListener(iphDelegate, scrollableContainerDelegate);
        listener.onScrollStateChanged(scrollState);

        if (expectEnabled) {
            Assert.assertTrue(mHasShownMenuIph);
        } else {
            Assert.assertFalse(mHasShownMenuIph);
        }
    }

    @Test
    @MediumTest
    @Feature({"Feed"})
    @ParameterAnnotations.UseMethodParameter(TestParamsForOnOffsetChanged.class)
    public void onScrollStateChanged_onHeaderOffsetChanged(boolean expectEnabled, int scrollState,
            int triggerState, boolean wouldTriggerHelpUI, int verticalScrollOffset,
            boolean isFeedHeaderPositionInRecyclerViewSuitableForIPH, boolean isFeedExpanded,
            boolean isSignedIn) {
        // Set Tracker mock.
        when(mTracker.getTriggerState(FeatureConstants.FEED_HEADER_MENU_FEATURE))
                .thenReturn(triggerState);
        when(mTracker.wouldTriggerHelpUI(FeatureConstants.FEED_HEADER_MENU_FEATURE))
                .thenReturn(wouldTriggerHelpUI);

        HeaderIphScrollListener.Delegate iphDelegate = new HeaderIphScrollListener.Delegate() {
            @Override
            public Tracker getFeatureEngagementTracker() {
                return mTracker;
            }
            @Override
            public void showMenuIph() {
                mHasShownMenuIph = true;
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
                new HeaderIphScrollListener(iphDelegate, scrollableContainerDelegate);
        listener.onHeaderOffsetChanged(-verticalScrollOffset);

        if (expectEnabled) {
            Assert.assertTrue(mHasShownMenuIph);
        } else {
            Assert.assertFalse(mHasShownMenuIph);
        }
    }
}
