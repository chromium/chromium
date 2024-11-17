// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.animation.AnimatorSet;
import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link FeedPlaceholderLayout}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedPlaceholderLayoutTest {
    @Mock AnimatorSet mAnimatorSet;

    private static class TestFeedPlaceholderLayout extends FeedPlaceholderLayout {
        // FeedPlaceholderLayout regards itself as visible if both isShown() and
        // isAttachedToWindow() are true. In order to reduce the number of test cases we will
        // control both at the same time with this variable.
        boolean mVisible;

        TestFeedPlaceholderLayout(Context context) {
            super(context, Robolectric.buildAttributeSet().build());
        }

        @Override
        public boolean isShown() {
            return mVisible;
        }

        @Override
        public boolean isAttachedToWindow() {
            return mVisible;
        }
    }

    TestFeedPlaceholderLayout mLayout;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mLayout = new TestFeedPlaceholderLayout(ApplicationProvider.getApplicationContext());
        mLayout.setAnimatorSetForTesting(mAnimatorSet);
    }

    @Test
    @SmallTest
    public void testOnVisibilityChanged_notStarted_notVisible() {
        doReturn(false).when(mAnimatorSet).isStarted();
        mLayout.mVisible = false;

        mLayout.onVisibilityChanged(mLayout, 0);

        // Do nothing because the animation wasn't started and the view wasn't visible.
        verify(mAnimatorSet, times(0)).start();
        verify(mAnimatorSet, times(0)).cancel();
    }

    @Test
    @SmallTest
    public void testOnVisibilityChanged_notStarted_visible() {
        doReturn(false).when(mAnimatorSet).isStarted();
        mLayout.mVisible = true;

        mLayout.onVisibilityChanged(mLayout, 0);

        // Start because the view is attached and visible and the animation hasn't started.
        verify(mAnimatorSet, times(1)).start();
        verify(mAnimatorSet, times(0)).cancel();
    }

    @Test
    @SmallTest
    public void testOnVisibilityChanged_started_notVisible() {
        doReturn(true).when(mAnimatorSet).isStarted();
        mLayout.mVisible = false;

        mLayout.onVisibilityChanged(mLayout, 0);

        // Cancel because the the animation is started and the view is no longer visible.
        verify(mAnimatorSet, times(0)).start();
        verify(mAnimatorSet, times(1)).cancel();
    }

    @Test
    @SmallTest
    public void testOnVisibilityChanged_started_visible() {
        doReturn(true).when(mAnimatorSet).isStarted();
        mLayout.mVisible = true;

        mLayout.onVisibilityChanged(mLayout, 0);

        // Do nothing because the animation is already started and the view is visible.
        verify(mAnimatorSet, times(0)).start();
        verify(mAnimatorSet, times(0)).cancel();
    }

    @Test
    @SmallTest
    public void testOnAttachedToWindow_notStarted_notVisible() {
        doReturn(false).when(mAnimatorSet).isStarted();
        mLayout.mVisible = false;

        mLayout.onAttachedToWindow();

        // Do nothing because the animation wasn't started and the view wasn't visible.
        verify(mAnimatorSet, times(0)).start();
        verify(mAnimatorSet, times(0)).cancel();
    }

    @Test
    @SmallTest
    public void testOnAttachedToWindow_notStarted_visible() {
        doReturn(false).when(mAnimatorSet).isStarted();
        mLayout.mVisible = true;

        mLayout.onAttachedToWindow();

        // Start because the view is attached and visible and the animation hasn't started.
        verify(mAnimatorSet, times(1)).start();
        verify(mAnimatorSet, times(0)).cancel();
    }

    @Test
    @SmallTest
    public void testOnAttachedToWindow_started_notVisible() {
        doReturn(true).when(mAnimatorSet).isStarted();
        mLayout.mVisible = false;

        mLayout.onAttachedToWindow();

        // Cancel because the animation was started and the view was not visible (even though it was
        // attached).
        verify(mAnimatorSet, times(0)).start();
        verify(mAnimatorSet, times(1)).cancel();
    }

    @Test
    @SmallTest
    public void testOnAttachedToWindow_started_visible() {
        doReturn(true).when(mAnimatorSet).isStarted();
        mLayout.mVisible = true;

        mLayout.onAttachedToWindow();

        // Do nothing; animation is already started and visible.
        verify(mAnimatorSet, times(0)).start();
        verify(mAnimatorSet, times(0)).cancel();
    }

    @Test
    @SmallTest
    public void testOnDetachedFromWindow_notStarted_notVisible() {
        doReturn(false).when(mAnimatorSet).isStarted();
        mLayout.mVisible = false;

        mLayout.onDetachedFromWindow();

        // Do nothing because the animation wasn't started and the view was detached.
        verify(mAnimatorSet, times(0)).start();
        verify(mAnimatorSet, times(0)).cancel();
    }

    @Test
    @SmallTest
    public void testOnDetachedFromWindow_notStarted_visible() {
        doReturn(false).when(mAnimatorSet).isStarted();
        mLayout.mVisible = true;

        mLayout.onDetachedFromWindow();

        // Do nothing because the animation wasn't started and the view was detached.
        verify(mAnimatorSet, times(0)).start();
        verify(mAnimatorSet, times(0)).cancel();
    }

    @Test
    @SmallTest
    public void testOnDetachedFromWindow_started_notVisible() {
        doReturn(true).when(mAnimatorSet).isStarted();
        mLayout.mVisible = false;

        mLayout.onDetachedFromWindow();

        // Cancel because the animation was started and the view was detached.
        verify(mAnimatorSet, times(0)).start();
        verify(mAnimatorSet, times(1)).cancel();
    }

    @Test
    @SmallTest
    public void testOnDetachedFromWindow_started_visible() {
        doReturn(true).when(mAnimatorSet).isStarted();
        mLayout.mVisible = true;

        mLayout.onDetachedFromWindow();

        // Cancel because the animation was started and the view was detached.
        verify(mAnimatorSet, times(0)).start();
        verify(mAnimatorSet, times(1)).cancel();
    }
}
