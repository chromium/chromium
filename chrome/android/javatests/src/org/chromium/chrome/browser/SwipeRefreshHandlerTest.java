// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout;
import org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout.OnRefreshListener;
import org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout.OnResetListener;
import org.chromium.ui.OverscrollAction;
import org.chromium.ui.base.BackGestureEventSwipeEdge;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests {@link SwipeRefreshHandler}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class SwipeRefreshHandlerTest {
    private static final int ACCESSIBILITY_SWIPE_REFRESH_STRING_ID =
            R.string.accessibility_swipe_refresh;

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    // Can't initialize here since myActivityTestRule.getActivity() is still null.
    private static String sAccessibilitySwipeRefreshString;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock private Tab mTab;
    private OnRefreshListener mOnRefreshListener;
    private OnResetListener mOnResetListener;
    private SwipeRefreshLayout mSwipeRefreshLayout;

    private SwipeRefreshHandler mHandler;

    private final SwipeRefreshHandler.SwipeRefreshLayoutCreator mSwipeRefreshLayoutCreator =
            context -> {
                mSwipeRefreshLayout = mock();
                doAnswer((invocation) -> mOnRefreshListener = invocation.getArgument(0))
                        .when(mSwipeRefreshLayout)
                        .setOnRefreshListener(any());
                doAnswer((invocation) -> mOnResetListener = invocation.getArgument(0))
                        .when(mSwipeRefreshLayout)
                        .setOnResetListener(any());
                return mSwipeRefreshLayout;
            };

    @BeforeClass
    public static void setUpSuite() {
        activityTestRule.launchActivity(null);
        sAccessibilitySwipeRefreshString =
                activityTestRule.getActivity().getString(ACCESSIBILITY_SWIPE_REFRESH_STRING_ID);
    }

    @Before
    public void setup() {
        when(mTab.getContext()).thenReturn(activityTestRule.getActivity());
        when(mTab.getUserDataHost()).thenReturn(new UserDataHost());
        when(mTab.getContentView()).thenReturn(mock());

        // Limited use of spy() so we can test actual object, while changing some behaviors
        // dynamically (such as whether mouse is attached or not)
        mHandler = spy(SwipeRefreshHandler.from(mTab, mSwipeRefreshLayoutCreator));
        mHandler.initWebContents(mock()); // Needed to enable the overscroll refresh handler.
        doReturn(true).when(mHandler).isRefreshOnOverscrollSupported(); // Default no mouse/touchpad
    }

    @Test
    @SmallTest
    public void testAccessibilityAnnouncement() {
        triggerRefresh(mHandler);

        InOrder orderVerifier = inOrder(mSwipeRefreshLayout);
        orderVerifier
                .verify(mSwipeRefreshLayout, times(1))
                .setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        orderVerifier
                .verify(mSwipeRefreshLayout, times(1))
                .setContentDescription(sAccessibilitySwipeRefreshString);

        reset(mHandler);

        orderVerifier.verify(mSwipeRefreshLayout, times(1)).setContentDescription(null);
    }

    /** Ensures that we do not trigger refresh if precision pointing device is attached */
    @Test
    @SmallTest
    public void testOverscrollButNoRefresh() {
        doReturn(false).when(mHandler).isRefreshOnOverscrollSupported(); // pointer device attached
        triggerRefresh(mHandler);
        // When refresh is NOT triggered, then refresh layout is NOT created
        assertNull(mSwipeRefreshLayout);
    }

    @Test
    @SmallTest
    public void testAccessibilityAnnouncement_swipingASecondTime() {
        triggerRefresh(mHandler);

        var firstSwipeRefreshLayout = mSwipeRefreshLayout;

        InOrder orderVerifier = inOrder(firstSwipeRefreshLayout);
        orderVerifier
                .verify(firstSwipeRefreshLayout, times(1))
                .setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        orderVerifier
                .verify(firstSwipeRefreshLayout, times(1))
                .setContentDescription(sAccessibilitySwipeRefreshString);

        reset(mHandler);

        orderVerifier.verify(mSwipeRefreshLayout, times(1)).setContentDescription(null);

        triggerRefresh(mHandler);

        var secondSwipeRefreshLayout = mSwipeRefreshLayout;

        // Assert that the swipe refresh layout objects are the same, i.e. we don't create a new
        // swipe refresh layout if we already have one.
        assertEquals(firstSwipeRefreshLayout, secondSwipeRefreshLayout);

        orderVerifier
                .verify(firstSwipeRefreshLayout, times(1))
                .setContentDescription(sAccessibilitySwipeRefreshString);
    }

    /**
     * Triggers a refresh. We need to do this through the handler so that the SwipeRefreshLayout is
     * initialized.
     *
     * @param handler The {@link SwipeRefreshHandler} to use.
     */
    private void triggerRefresh(SwipeRefreshHandler handler) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        handler.start(
                                OverscrollAction.PULL_TO_REFRESH,
                                // The left/right swipe direction is arbitrary for an action type of
                                // PULL_TO_REFRESH.
                                BackGestureEventSwipeEdge.LEFT));
        // # of pixels (of reasonably small value) which a finger moves across per one motion event.
        final float distancePx = 6.0f;
        for (int numPullSteps = 0; numPullSteps < 10; numPullSteps++) {
            ThreadUtils.runOnUiThreadBlocking(() -> handler.pull(0, distancePx));
        }
        if (mOnRefreshListener != null) mOnRefreshListener.onRefresh();
    }

    /**
     * Resets the refresh overscroll.
     *
     * @param handler The {@link SwipeRefreshHandler} to use.
     */
    private void reset(SwipeRefreshHandler handler) {
        ThreadUtils.runOnUiThreadBlocking(handler::reset);
        if (mOnResetListener != null) mOnResetListener.onReset();
    }
}
