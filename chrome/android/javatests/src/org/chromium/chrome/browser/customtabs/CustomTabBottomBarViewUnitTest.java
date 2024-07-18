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

import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/** On device unit tests for {@link CustomTabBottomBarView}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class CustomTabBottomBarViewUnitTest extends BlankUiTestActivityTestCase {
    @Mock private SwipeHandler mSwipeHandler;
    @Mock private OnClickListener mOnClickListener;

    private CustomTabBottomBarView mView;
    private View mStub;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mSwipeHandler.isSwipeEnabled(eq(ScrollDirection.UP))).thenReturn(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView =
                            (CustomTabBottomBarView)
                                    getActivity()
                                            .getLayoutInflater()
                                            .inflate(R.layout.custom_tabs_bottombar, null);
                    mStub =
                            getActivity()
                                    .getLayoutInflater()
                                    .inflate(R.layout.bottombar_stub, null);
                    mStub.setOnClickListener(mOnClickListener);
                    mView.addView(mStub);
                    mView.setSwipeHandler(mSwipeHandler);
                    getActivity().setContentView(mView);
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
    @DisabledTest(message = "crbug.com/329163715")
    public void testSwipeUp() {
        onView(withChild(withId(R.id.stub))).perform(swipeUp());
        verify(mSwipeHandler).onSwipeStarted(eq(ScrollDirection.UP), any(MotionEvent.class));
    }

    @Test
    @SmallTest
    @DisableIf.Build(
            supported_abis_includes = "arm64-v8a",
            sdk_is_greater_than = 33,
            message = "crbug.com/353773627")
    public void testSwipeRightDoesNotTrigger() {
        onView(withChild(withId(R.id.stub))).perform(swipeRight());
        verify(mSwipeHandler, never()).onSwipeStarted(anyInt(), any());
    }

    @Test
    @SmallTest
    public void testChildRespondsToClick() {
        onView(withId(R.id.stub)).perform(click());
        verify(mOnClickListener).onClick(eq(mStub));
    }
}
