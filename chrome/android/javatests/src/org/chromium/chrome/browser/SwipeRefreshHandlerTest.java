// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableLeakChecks;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout;
import org.chromium.ui.OverscrollAction;
import org.chromium.ui.OverscrollActivationStatus;
import org.chromium.ui.base.BackGestureEventSwipeEdge;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests {@link SwipeRefreshHandler}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@DisableLeakChecks("crbug.com/527130930")
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
    private ContentView mContentView;
    private SwipeRefreshLayout mSwipeRefreshLayout;
    private final SwipeRefreshHandler.SwipeRefreshLayoutCreator mSwipeRefreshLayoutCreator =
            context -> {
                mSwipeRefreshLayout = new SwipeRefreshLayout(context);
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
        Activity activity = activityTestRule.getActivity();
        when(mTab.getContext()).thenReturn(activity);
        when(mTab.getUserDataHost()).thenReturn(new UserDataHost());
        // Attached to the window so that the refresh animation runs.
        mContentView =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            ContentView contentView =
                                    ContentView.createContentView(
                                            activity, /* webContents= */ null);
                            activity.setContentView(contentView);
                            return contentView;
                        });
        when(mTab.getContentView()).thenReturn(mContentView);
    }

    @Test
    @SmallTest
    public void testAccessibilityAnnouncement() {
        var handler = SwipeRefreshHandler.from(mTab, mSwipeRefreshLayoutCreator);
        handler.initWebContents(mock()); // Needed to enable the overscroll refresh handler.
        triggerRefresh(handler);

        assertEquals(
                View.ACCESSIBILITY_LIVE_REGION_POLITE,
                mSwipeRefreshLayout.getAccessibilityLiveRegion());
        assertEquals(sAccessibilitySwipeRefreshString, mSwipeRefreshLayout.getContentDescription());

        reset(handler);

        assertNull(mSwipeRefreshLayout.getContentDescription());
    }

    @Test
    @SmallTest
    public void testAccessibilityAnnouncement_swipingASecondTime() {
        var handler = SwipeRefreshHandler.from(mTab, mSwipeRefreshLayoutCreator);
        handler.initWebContents(mock()); // Needed to enable the overscroll refresh handler.
        triggerRefresh(handler);

        var firstSwipeRefreshLayout = mSwipeRefreshLayout;

        assertEquals(
                View.ACCESSIBILITY_LIVE_REGION_POLITE,
                firstSwipeRefreshLayout.getAccessibilityLiveRegion());
        assertEquals(
                sAccessibilitySwipeRefreshString, firstSwipeRefreshLayout.getContentDescription());

        reset(handler);

        assertNull(firstSwipeRefreshLayout.getContentDescription());

        triggerRefresh(handler);

        var secondSwipeRefreshLayout = mSwipeRefreshLayout;

        // Assert that the swipe refresh layout objects are the same, i.e. we don't create a new
        // swipe refresh layout if we already have one.
        assertEquals(firstSwipeRefreshLayout, secondSwipeRefreshLayout);

        assertEquals(
                sAccessibilitySwipeRefreshString, firstSwipeRefreshLayout.getContentDescription());
    }

    @Test
    @SmallTest
    public void testSideUiWidths_passedToSwipeRefreshLayout() {
        var handler = SwipeRefreshHandler.from(mTab, mSwipeRefreshLayoutCreator);
        handler.initWebContents(mock());
        handler.setSideUiWidthsForTesting(100, 200);
        triggerRefresh(handler);

        // The spinner is centered between the side UIs.
        View spinner = mSwipeRefreshLayout.getChildAt(0);
        int width = mSwipeRefreshLayout.getWidth();
        assertEquals((100 + width - 200) / 2f, (spinner.getLeft() + spinner.getRight()) / 2f, 1f);
    }

    /**
     * Triggers a refresh through the handler and waits for the refresh listener to run.
     *
     * @param handler The {@link SwipeRefreshHandler} to use.
     */
    private void triggerRefresh(SwipeRefreshHandler handler) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    handler.start(
                            OverscrollAction.PULL_TO_REFRESH,
                            // The left/right swipe direction is arbitrary for an action type of
                            // PULL_TO_REFRESH.
                            BackGestureEventSwipeEdge.LEFT);
                    // Pull far enough for the release to start a refresh.
                    for (int numPullSteps = 0; numPullSteps < 10; numPullSteps++) {
                        handler.pull(0, 1000);
                    }
                    handler.release(OverscrollActivationStatus.ALLOW_ACTIVATION);
                });
        // The refresh listener sets the content description when the refresh animation ends.
        CriteriaHelper.pollUiThread(() -> mSwipeRefreshLayout.getContentDescription() != null);
    }

    /**
     * Resets the refresh overscroll.
     *
     * @param handler The {@link SwipeRefreshHandler} to use.
     */
    private void reset(SwipeRefreshHandler handler) {
        ThreadUtils.runOnUiThreadBlocking(handler::reset);
    }
}
