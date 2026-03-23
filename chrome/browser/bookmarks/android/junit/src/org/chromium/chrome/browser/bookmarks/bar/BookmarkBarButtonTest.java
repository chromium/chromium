// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.util.ClickWithMetaStateCallback;

/** Unit tests for the {@link BookmarkBarButton}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarButtonTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ClickWithMetaStateCallback mClickCallback;

    private Activity mActivity;
    private BookmarkBarButton mButton;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        mButton =
                (BookmarkBarButton)
                        LayoutInflater.from(mActivity).inflate(R.layout.bookmark_bar_button, null);
        mButton.setClickCallback(mClickCallback);
    }

    @Test
    @SmallTest
    public void testOnGenericMotionEvent_MiddleClick() {
        // Initial press to set the button state.
        MotionEvent pressEvent = Mockito.mock(MotionEvent.class);
        when(pressEvent.getSource()).thenReturn(InputDevice.SOURCE_MOUSE);
        when(pressEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_BUTTON_PRESS);
        when(pressEvent.getActionButton()).thenReturn(MotionEvent.BUTTON_TERTIARY);
        when(pressEvent.getButtonState()).thenReturn(MotionEvent.BUTTON_TERTIARY);
        mButton.onGenericMotionEvent(pressEvent);

        // Release event triggers the callback.
        MotionEvent releaseEvent = Mockito.mock(MotionEvent.class);
        when(releaseEvent.getSource()).thenReturn(InputDevice.SOURCE_MOUSE);
        when(releaseEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_BUTTON_RELEASE);
        when(releaseEvent.getActionButton()).thenReturn(MotionEvent.BUTTON_TERTIARY);
        when(releaseEvent.getMetaState()).thenReturn(KeyEvent.META_CTRL_ON);
        when(releaseEvent.getButtonState()).thenReturn(0);

        assertTrue(mButton.onGenericMotionEvent(releaseEvent));
        verify(mClickCallback).onClickWithMeta(KeyEvent.META_CTRL_ON, MotionEvent.BUTTON_TERTIARY);
    }

    @Test
    @SmallTest
    public void testOnGenericMotionEvent_NotMiddleClick() {
        MotionEvent event = Mockito.mock(MotionEvent.class);
        when(event.getSource()).thenReturn(InputDevice.SOURCE_MOUSE);
        when(event.getActionMasked()).thenReturn(MotionEvent.ACTION_BUTTON_RELEASE);
        when(event.getActionButton()).thenReturn(MotionEvent.BUTTON_PRIMARY);
        when(event.getMetaState()).thenReturn(0);
        when(event.getButtonState()).thenReturn(MotionEvent.BUTTON_PRIMARY);

        mButton.onGenericMotionEvent(event);
        verify(mClickCallback, never()).onClickWithMeta(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testOnTouchEvent_MiddleClickConsumedAndCleared() {
        MotionEvent downEvent = Mockito.mock(MotionEvent.class);
        when(downEvent.getButtonState()).thenReturn(MotionEvent.BUTTON_TERTIARY);
        when(downEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_DOWN);

        assertTrue("Middle click down should be consumed", mButton.onTouchEvent(downEvent));

        MotionEvent upEvent = Mockito.mock(MotionEvent.class);
        when(upEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_UP);
        assertTrue("Middle click up should be consumed", mButton.onTouchEvent(upEvent));

        // After UP, mLastEventButtonState should be 0, so performClick should work normally
        // (if it were called by the system, which it won't be because we consumed DOWN).
        mButton.performClick();
        verify(mClickCallback).onClickWithMeta(anyInt(), eq(0));
    }

    @Test
    @SmallTest
    public void testOnClick_FiresForPrimaryClick() {
        // First simulate a primary click down in onTouchEvent to set state.
        MotionEvent downEvent = Mockito.mock(MotionEvent.class);
        when(downEvent.getButtonState()).thenReturn(MotionEvent.BUTTON_PRIMARY);
        when(downEvent.getMetaState()).thenReturn(0);
        when(downEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_DOWN);
        mButton.onTouchEvent(downEvent);

        // Simulate an UP event with a different meta state to ensure it's captured
        // but that the button state persists.
        MotionEvent upEvent = Mockito.mock(MotionEvent.class);
        when(upEvent.getButtonState()).thenReturn(0); // UP usually has 0 button state
        when(upEvent.getMetaState()).thenReturn(KeyEvent.META_SHIFT_ON);
        when(upEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_UP);
        mButton.onTouchEvent(upEvent);

        // Now trigger the standard click.
        mButton.performClick();

        verify(mClickCallback).onClickWithMeta(KeyEvent.META_SHIFT_ON, MotionEvent.BUTTON_PRIMARY);
    }
}
