// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;
import android.view.MotionEvent;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TabListRecyclerView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListRecyclerViewUnitTest {
    private Activity mActivity;
    private TestTabListRecyclerView mRecyclerView;

    private static class TestTabListRecyclerView extends TabListRecyclerView {
        public TestTabListRecyclerView(Context context) {
            super(context, null);
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mRecyclerView = new TestTabListRecyclerView(mActivity);
    }

    private boolean getSuspendedScrollBarEnabled(TabListRecyclerView view) {
        return ReflectionHelpers.getField(view, "mSuspendedScrollBarEnabled");
    }

    @Test
    @SmallTest
    public void testHoverEnterAndExit_ScrollbarEnabled() {
        mRecyclerView.setVerticalScrollBarEnabled(true);
        assertTrue(mRecyclerView.isVerticalScrollBarEnabled());

        // Hover Enter
        MotionEvent enterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        mRecyclerView.dispatchHoverEvent(enterEvent);

        assertFalse(mRecyclerView.isVerticalScrollBarEnabled());
        assertTrue(getSuspendedScrollBarEnabled(mRecyclerView));

        // Hover Move (should not toggle again)
        MotionEvent moveEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_MOVE, 10f, 10f, 0);
        mRecyclerView.dispatchHoverEvent(moveEvent);

        // Hover Exit
        MotionEvent exitEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 0f, 0f, 0);
        mRecyclerView.dispatchHoverEvent(exitEvent);

        assertTrue(mRecyclerView.isVerticalScrollBarEnabled());
        assertFalse(getSuspendedScrollBarEnabled(mRecyclerView));
    }

    @Test
    @SmallTest
    public void testHoverEnterAndExit_ScrollbarDisabled() {
        mRecyclerView.setVerticalScrollBarEnabled(false);
        assertFalse(mRecyclerView.isVerticalScrollBarEnabled());

        // Hover Enter
        MotionEvent enterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        mRecyclerView.dispatchHoverEvent(enterEvent);

        assertFalse(mRecyclerView.isVerticalScrollBarEnabled());
        assertFalse(getSuspendedScrollBarEnabled(mRecyclerView));

        // Hover Exit
        MotionEvent exitEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 0f, 0f, 0);
        mRecyclerView.dispatchHoverEvent(exitEvent);

        assertFalse(mRecyclerView.isVerticalScrollBarEnabled());
    }

    @Test
    @SmallTest
    public void testDetachedFromWindow_RestoresScrollbar() {
        mRecyclerView.setVerticalScrollBarEnabled(true);

        // Hover Enter to suspend
        MotionEvent enterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        mRecyclerView.dispatchHoverEvent(enterEvent);
        assertTrue(getSuspendedScrollBarEnabled(mRecyclerView));

        // Detach from window
        mRecyclerView.onDetachedFromWindow();

        assertTrue(mRecyclerView.isVerticalScrollBarEnabled());
        assertFalse(getSuspendedScrollBarEnabled(mRecyclerView));
    }

    @Test
    @SmallTest
    public void testSetVerticalScrollBarEnabledFalse_WhileSuspended() {
        mRecyclerView.setVerticalScrollBarEnabled(true);

        // Hover Enter to suspend
        MotionEvent enterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        mRecyclerView.dispatchHoverEvent(enterEvent);
        assertTrue(getSuspendedScrollBarEnabled(mRecyclerView));

        // Call setVerticalScrollBarEnabled(false) externally
        mRecyclerView.setVerticalScrollBarEnabled(false);
        assertFalse(getSuspendedScrollBarEnabled(mRecyclerView));
        assertFalse(mRecyclerView.isVerticalScrollBarEnabled());

        // Hover Exit (should not restore)
        MotionEvent exitEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 0f, 0f, 0);
        mRecyclerView.dispatchHoverEvent(exitEvent);

        assertFalse(mRecyclerView.isVerticalScrollBarEnabled());
    }
}
