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

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link CustomTabBottomBarView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CustomTabBottomBarViewUnitTest {
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
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();

        mView =
                (CustomTabBottomBarView)
                        mActivity.getLayoutInflater().inflate(R.layout.custom_tabs_bottombar, null);
        mStub = mActivity.getLayoutInflater().inflate(R.layout.bottombar_stub, null);
        mStub.setOnClickListener(mClickHelper::notifyCalled);
        mView.addView(mStub);
        mView.setSwipeHandler(mSwipeHandler);
        mActivity.setContentView(mView);
    }

    @Test
    public void testTouchEventNotInterceptedWithNoSwipeHandler() {
        mView.setSwipeHandler(null);
        var motionEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        assertFalse(mView.onInterceptTouchEvent(motionEvent));
        assertFalse(mView.onTouchEvent(motionEvent));
        motionEvent.recycle();
    }

    @Test
    public void testSwipeUp() {
        onView(withChild(withId(R.id.stub))).perform(swipeUp());
        assertEquals(
                Integer.valueOf(ScrollDirection.UP),
                mSwipeDirectionHelper.getOnlyPayloadBlocking());
    }

    @Test
    @DisableIf.Build(
            supported_abis_includes = "arm64-v8a",
            sdk_is_greater_than = 33,
            message = "crbug.com/353773627")
    public void testSwipeRightDoesNotTrigger() {
        onView(withChild(withId(R.id.stub))).perform(swipeRight());
        assertEquals(0, mSwipeDirectionHelper.getCallCount());
    }

    @Test
    public void testChildRespondsToClick() {
        onView(withId(R.id.stub)).perform(click());
        assertEquals(mStub, mClickHelper.getOnlyPayloadBlocking());
    }
}
