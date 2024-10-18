// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.automotivetoolbar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.GestureDetector;
import android.view.MotionEvent;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

@RunWith(BaseRobolectricTestRunner.class)
public class EdgeSwipeGestureDetectorUnitTest {
    private EdgeSwipeGestureDetector mEdgeSwipeGestureDetector;
    private GestureDetector.SimpleOnGestureListener mSimpleOnGestureListener;
    private Activity mActivity;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private AutomotiveBackButtonToolbarCoordinator.OnSwipeCallback mOnSwipeCallback;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        mActivity = activity;
        mEdgeSwipeGestureDetector = new EdgeSwipeGestureDetector(activity, mOnSwipeCallback);
        mSimpleOnGestureListener = mEdgeSwipeGestureDetector.getSwipeGestureListenerForTesting();
    }

    @Test
    public void onSwipe_validSwipe() {
        MotionEvent eventSrc = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 7, 0, 0);
        MotionEvent eventTo = MotionEvent.obtain(0, 100, MotionEvent.ACTION_MOVE, 300, 0, 0);
        assertTrue(mSimpleOnGestureListener.onScroll(eventSrc, eventTo, -300, 0));
        verify(mOnSwipeCallback).handleSwipe();
    }

    @Test
    public void onSwipe_invalidSwipe() {
        MotionEvent eventSrc = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 300, 0, 0);
        MotionEvent eventTo = MotionEvent.obtain(0, 100, MotionEvent.ACTION_MOVE, 0, 0, 0);
        assertFalse(
                "Swipe in opposite direction should not be consumed",
                mSimpleOnGestureListener.onScroll(eventSrc, eventTo, 300, 0));

        MotionEvent eventSrc1 = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 300, 0, 0);
        MotionEvent eventTo1 = MotionEvent.obtain(0, 100, MotionEvent.ACTION_MOVE, 600, 0, 0);
        assertFalse(
                "Swipe does not start on left edge",
                mSimpleOnGestureListener.onScroll(eventSrc1, eventTo1, -300, 0));

        MotionEvent eventSrc2 = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        MotionEvent eventTo2 = MotionEvent.obtain(0, 100, MotionEvent.ACTION_MOVE, 40, 0, 0);
        assertFalse(
                "Swipe does not meet horizontal threshold",
                mSimpleOnGestureListener.onScroll(eventSrc2, eventTo2, -40, 0));

        MotionEvent eventSrc3 = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 7, 0, 0);
        MotionEvent eventTo3 = MotionEvent.obtain(0, 100, MotionEvent.ACTION_MOVE, 300, 500, 0);
        assertFalse(
                "Swipe has too much vertical delta",
                mSimpleOnGestureListener.onScroll(eventSrc3, eventTo3, -300, 500));
    }
}
