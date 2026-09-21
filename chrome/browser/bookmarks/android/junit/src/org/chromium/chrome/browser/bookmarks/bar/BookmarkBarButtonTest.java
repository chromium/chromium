// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Point;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

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
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.ClickWithMetaStateCallback;

/** Unit tests for the {@link BookmarkBarButton}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarButtonTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ClickWithMetaStateCallback mClickCallback;
    @Mock private View.OnLongClickListener mLongClickListener;
    @Mock private MotionEvent mPressEvent;
    @Mock private MotionEvent mReleaseEvent;
    @Mock private MotionEvent mMotionEvent;
    @Mock private MotionEvent mDownEvent;
    @Mock private MotionEvent mUpEvent;
    @Mock private View.OnLongClickListener mViewOnLongClickListener;

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
    public void testOnGenericMotionEvent_MiddleClick() {
        // Initial press to set the button state.
        when(mPressEvent.getSource()).thenReturn(InputDevice.SOURCE_MOUSE);
        when(mPressEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_BUTTON_PRESS);
        when(mPressEvent.getActionButton()).thenReturn(MotionEvent.BUTTON_TERTIARY);
        when(mPressEvent.getButtonState()).thenReturn(MotionEvent.BUTTON_TERTIARY);
        mButton.onGenericMotionEvent(mPressEvent);

        // Release event triggers the callback.
        when(mReleaseEvent.getSource()).thenReturn(InputDevice.SOURCE_MOUSE);
        when(mReleaseEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_BUTTON_RELEASE);
        when(mReleaseEvent.getActionButton()).thenReturn(MotionEvent.BUTTON_TERTIARY);
        when(mReleaseEvent.getMetaState()).thenReturn(KeyEvent.META_CTRL_ON);
        when(mReleaseEvent.getButtonState()).thenReturn(0);

        assertTrue(mButton.onGenericMotionEvent(mReleaseEvent));
        verify(mClickCallback).onClickWithMeta(KeyEvent.META_CTRL_ON, MotionEvent.BUTTON_TERTIARY);
    }

    @Test
    public void testOnGenericMotionEvent_RightClick() {
        // Initial press to set the button state.
        when(mPressEvent.getSource()).thenReturn(InputDevice.SOURCE_MOUSE);
        when(mPressEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_BUTTON_PRESS);
        when(mPressEvent.getActionButton()).thenReturn(MotionEvent.BUTTON_SECONDARY);
        when(mPressEvent.getButtonState()).thenReturn(MotionEvent.BUTTON_SECONDARY);
        mButton.onGenericMotionEvent(mPressEvent);

        // Release event triggers the callback.
        when(mReleaseEvent.getSource()).thenReturn(InputDevice.SOURCE_MOUSE);
        when(mReleaseEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_BUTTON_RELEASE);
        when(mReleaseEvent.getActionButton()).thenReturn(MotionEvent.BUTTON_SECONDARY);
        when(mReleaseEvent.getMetaState()).thenReturn(KeyEvent.META_CTRL_ON);
        when(mReleaseEvent.getButtonState()).thenReturn(0);

        assertTrue(mButton.onGenericMotionEvent(mReleaseEvent));
        verify(mClickCallback).onClickWithMeta(KeyEvent.META_CTRL_ON, MotionEvent.BUTTON_SECONDARY);
    }

    @Test
    public void testOnGenericMotionEvent_NotMiddleClick() {
        when(mMotionEvent.getSource()).thenReturn(InputDevice.SOURCE_MOUSE);
        when(mMotionEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_BUTTON_RELEASE);
        when(mMotionEvent.getActionButton()).thenReturn(MotionEvent.BUTTON_PRIMARY);
        when(mMotionEvent.getMetaState()).thenReturn(0);
        when(mMotionEvent.getButtonState()).thenReturn(MotionEvent.BUTTON_PRIMARY);

        mButton.onGenericMotionEvent(mMotionEvent);
        verify(mClickCallback, never()).onClickWithMeta(anyInt(), anyInt());
    }

    @Test
    public void testOnTouchEvent_MiddleClickConsumedAndCleared() {
        when(mDownEvent.getButtonState()).thenReturn(MotionEvent.BUTTON_TERTIARY);
        when(mDownEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_DOWN);

        assertTrue("Middle click down should be consumed", mButton.onTouchEvent(mDownEvent));

        when(mUpEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_UP);
        assertTrue("Middle click up should be consumed", mButton.onTouchEvent(mUpEvent));

        // After UP, mLastEventButtonState should be 0, so performClick should work normally
        // (if it were called by the system, which it won't be because we consumed DOWN).
        mButton.performClick();
        verify(mClickCallback).onClickWithMeta(anyInt(), eq(0));
    }

    @Test
    public void testOnClick_FiresForPrimaryClick() {
        // First simulate a primary click down in onTouchEvent to set state.
        when(mDownEvent.getButtonState()).thenReturn(MotionEvent.BUTTON_PRIMARY);
        when(mDownEvent.getMetaState()).thenReturn(0);
        when(mDownEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_DOWN);
        mButton.onTouchEvent(mDownEvent);

        // Simulate an UP event with a different meta state to ensure it's captured
        // but that the button state persists.
        when(mUpEvent.getButtonState()).thenReturn(0); // UP usually has 0 button state
        when(mUpEvent.getMetaState()).thenReturn(KeyEvent.META_SHIFT_ON);
        when(mUpEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_UP);
        mButton.onTouchEvent(mUpEvent);

        // Now trigger the standard click.
        mButton.performClick();

        verify(mClickCallback).onClickWithMeta(KeyEvent.META_SHIFT_ON, MotionEvent.BUTTON_PRIMARY);
    }

    @Test
    public void testOnLongClick_FiresLongClickListener() {
        when(mViewOnLongClickListener.onLongClick(mButton)).thenReturn(true);
        mButton.setOnLongClickListener(mViewOnLongClickListener);

        assertTrue(mButton.performLongClick());
        verify(mViewOnLongClickListener).onLongClick(mButton);
        verify(mClickCallback, never()).onClickWithMeta(anyInt(), anyInt());
    }

    @Test
    public void testOnTouchEvent_RightClickConsumedAndCleared() {
        when(mDownEvent.getButtonState()).thenReturn(MotionEvent.BUTTON_SECONDARY);
        when(mDownEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_DOWN);

        assertTrue("Right click down should be consumed", mButton.onTouchEvent(mDownEvent));

        when(mUpEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_UP);
        assertTrue("Right click up should be consumed", mButton.onTouchEvent(mUpEvent));

        // After UP, mLastEventButtonState should be 0, so performClick should work normally.
        mButton.performClick();
        verify(mClickCallback).onClickWithMeta(anyInt(), eq(0));
    }

    @Test
    public void testGetLastClickPoint() {
        when(mMotionEvent.getX()).thenReturn(123f);
        when(mMotionEvent.getY()).thenReturn(456f);
        when(mMotionEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_DOWN);

        mButton.onTouchEvent(mMotionEvent);

        Point point = mButton.getLastClickPoint();
        assertEquals(123, point.x);
        assertEquals(456, point.y);
    }

    @Test
    public void testDoubleTrigger_OnlyFiresOnce() {
        // Down event (touch)
        when(mDownEvent.getButtonState()).thenReturn(MotionEvent.BUTTON_TERTIARY);
        when(mDownEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_DOWN);
        assertTrue(mButton.onTouchEvent(mDownEvent));

        // Press event (generic)
        when(mPressEvent.getSource()).thenReturn(InputDevice.SOURCE_MOUSE);
        when(mPressEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_BUTTON_PRESS);
        when(mPressEvent.getActionButton()).thenReturn(MotionEvent.BUTTON_TERTIARY);
        when(mPressEvent.getButtonState()).thenReturn(MotionEvent.BUTTON_TERTIARY);
        mButton.onGenericMotionEvent(mPressEvent);

        // Release event (generic) -> should trigger click
        when(mReleaseEvent.getSource()).thenReturn(InputDevice.SOURCE_MOUSE);
        when(mReleaseEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_BUTTON_RELEASE);
        when(mReleaseEvent.getActionButton()).thenReturn(MotionEvent.BUTTON_TERTIARY);
        when(mReleaseEvent.getMetaState()).thenReturn(0);
        when(mReleaseEvent.getButtonState()).thenReturn(0);
        assertTrue(mButton.onGenericMotionEvent(mReleaseEvent));
        verify(mClickCallback).onClickWithMeta(0, MotionEvent.BUTTON_TERTIARY);

        // Reset mock to verify it's not called again
        Mockito.reset(mClickCallback);

        // Up event (touch) -> should NOT trigger click again
        when(mUpEvent.getActionMasked()).thenReturn(MotionEvent.ACTION_UP);
        assertTrue(mButton.onTouchEvent(mUpEvent));
        verify(mClickCallback, never()).onClickWithMeta(anyInt(), anyInt());
    }

    @Test
    public void testLongClickListenerProperty() {
        when(mLongClickListener.onLongClick(mButton)).thenReturn(true);

        PropertyModel model =
                new PropertyModel.Builder(BookmarkBarButtonProperties.ALL_KEYS)
                        .with(BookmarkBarButtonProperties.LONG_CLICK_LISTENER, mLongClickListener)
                        .build();
        PropertyModelChangeProcessor.create(model, mButton, BookmarkBarButtonViewBinder::bind);

        assertTrue(mButton.performLongClick());
        verify(mLongClickListener).onLongClick(mButton);
    }
}
