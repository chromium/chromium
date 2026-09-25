// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.swipeRight;
import static androidx.test.espresso.action.ViewActions.swipeUp;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import android.app.Activity;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** On device unit tests for {@link CustomTabBottomBarView}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class CustomTabBottomBarViewUnitTest {
    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private final PayloadCallbackHelper<Integer> mSwipeDirectionHelper =
            new PayloadCallbackHelper<>();
    private final PayloadCallbackHelper<View> mClickHelper = new PayloadCallbackHelper<>();
    private final SwipeHandler mSwipeHandler =
            new SwipeHandler() {
                @Override
                public void onSwipeStarted(
                        @ScrollDirection int direction, MotionEvent triggerEvent) {
                    mSwipeDirectionHelper.notifyCalled(direction);
                }

                @Override
                public boolean isSwipeEnabled(
                        @ScrollDirection int direction, MotionEvent triggerEvent) {
                    return direction == ScrollDirection.UP;
                }
            };

    private Activity mActivity;
    private CustomTabBottomBarView mView;
    private View mStub;

    @Before
    public void setUp() {
        mActivity = mActivityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView =
                            (CustomTabBottomBarView)
                                    mActivity
                                            .getLayoutInflater()
                                            .inflate(R.layout.custom_tabs_bottombar, null);
                    mStub = mActivity.getLayoutInflater().inflate(R.layout.bottombar_stub, null);
                    mStub.setOnClickListener(mClickHelper::notifyCalled);
                    mView.addView(mStub);
                    mView.setSwipeHandler(mSwipeHandler);
                    mActivity.setContentView(mView);
                });
    }

    @Test
    @SmallTest
    public void testTouchEventNotInterceptedWithNoSwipeHandler() {
        mView.setSwipeHandler(null);
        var motionEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        assertFalse(mView.onInterceptTouchEvent(motionEvent));
        assertFalse(mView.onTouchEvent(motionEvent));
        motionEvent.recycle();
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/563056275")
    public void testSwipeUp() {
        onView(withChild(withId(R.id.stub))).perform(swipeUp());
        assertEquals(
                Integer.valueOf(ScrollDirection.UP),
                mSwipeDirectionHelper.getOnlyPayloadBlocking());
    }

    @Test
    @SmallTest
    @DisableIf.Build(
            supported_abis_includes = "arm64-v8a",
            sdk_is_greater_than = 33,
            message = "crbug.com/353773627")
    public void testSwipeRightDoesNotTrigger() {
        onView(withChild(withId(R.id.stub))).perform(swipeRight());
        assertEquals(0, mSwipeDirectionHelper.getCallCount());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/563056275")
    public void testChildRespondsToClick() {
        onView(withId(R.id.stub)).perform(click());
        assertEquals(mStub, mClickHelper.getOnlyPayloadBlocking());
    }
}
