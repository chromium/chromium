// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.view.MotionEvent;
import android.widget.HorizontalScrollView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.tile.TileView;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link TileDragDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TileDragDelegateImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TileDragSession.EventListener mEventListener;

    private Activity mActivity;
    private MostVisitedTilesLayout mMvTilesLayout;
    private TileView mDraggableTileView1;
    private TileView mDraggableTileView2;
    private TileView mNonDraggableTileView;
    private TileDragDelegateImpl mDelegate;

    private static class TestTileView extends TileView {
        private final boolean mIsDraggable;

        public TestTileView(Context context, boolean isDraggable) {
            super(context, null);
            mIsDraggable = isDraggable;
        }

        @Override
        public boolean isDraggable() {
            return mIsDraggable;
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        HorizontalScrollView scrollView = new HorizontalScrollView(mActivity);
        mMvTilesLayout = new MostVisitedTilesLayout(mActivity, null);
        scrollView.addView(mMvTilesLayout);

        mDraggableTileView1 = new TestTileView(mActivity, /* isDraggable= */ true);
        mDraggableTileView2 = new TestTileView(mActivity, /* isDraggable= */ true);
        mNonDraggableTileView = new TestTileView(mActivity, /* isDraggable= */ false);

        mMvTilesLayout.addTile(mDraggableTileView1);
        mMvTilesLayout.addTile(mDraggableTileView2);
        mMvTilesLayout.addTile(mNonDraggableTileView);

        mDelegate = new TileDragDelegateImpl(mMvTilesLayout);
    }

    @Test
    public void testHasTileDragSession_FalseInitially() {
        assertFalse(mDelegate.hasTileDragSession());
    }

    @Test
    public void testOnTileTouchDown_DraggableTile_StartsSession() {
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mDraggableTileView1, event, mEventListener);

        assertTrue(mDelegate.hasTileDragSession());
    }

    @Test
    public void testOnTileTouchDown_NonDraggableTile_DoesNotStartSession() {
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mNonDraggableTileView, event, mEventListener);

        assertFalse(mDelegate.hasTileDragSession());
    }

    @Test
    public void testOnSessionTileTouch_WhenNoSession_DoesNotThrowNpe() {
        assertFalse(mDelegate.hasTileDragSession());
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0);

        // Calling onSessionTileTouch without an active session should cleanly no-op.
        mDelegate.onSessionTileTouch(mDraggableTileView1, event);

        assertFalse(mDelegate.hasTileDragSession());
        verify(mEventListener, never()).onReorderCancel();
    }

    @Test
    public void testOnSessionTileTouch_WhenDifferentView_Ignored() {
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mDraggableTileView1, downEvent, mEventListener);
        assertTrue(mDelegate.hasTileDragSession());

        // Event for a different view should be ignored.
        MotionEvent upEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0);
        mDelegate.onSessionTileTouch(mDraggableTileView2, upEvent);

        // Session for mDraggableTileView1 remains active.
        assertTrue(mDelegate.hasTileDragSession());
    }

    @Test
    public void testOnTileTouchDown_NonDraggableTile_CancelsActiveSession() {
        MotionEvent downEvent1 = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mDraggableTileView1, downEvent1, mEventListener);
        assertTrue(mDelegate.hasTileDragSession());

        // Multi-touch: Touching a non-draggable tile cancels the active drag session.
        MotionEvent downEvent2 = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mNonDraggableTileView, downEvent2, mEventListener);

        assertFalse(mDelegate.hasTileDragSession());
    }

    @Test
    public void testDropAnimation_HasTileDragSessionIsFalseAndTouchNonDraggableDoesNotCrash() {
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mDraggableTileView1, downEvent, mEventListener);
        assertTrue(mDelegate.hasTileDragSession());

        // Advance timer to trigger START phase.
        ShadowLooper.idleMainLooper(300, TimeUnit.MILLISECONDS);
        assertTrue(mDelegate.hasTileDragSession());

        // Release finger: ACTION_UP.
        MotionEvent upEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0);
        mDelegate.onSessionTileTouch(mDraggableTileView1, upEvent);

        // Session has finished; hasTileDragSession() MUST be false despite pending finalizer.
        assertFalse(mDelegate.hasTileDragSession());

        // Interacting with a non-draggable tile during this window should not crash.
        MotionEvent nonDraggableDown = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mNonDraggableTileView, nonDraggableDown, mEventListener);
        assertFalse(mDelegate.hasTileDragSession());

        MotionEvent nonDraggableUp = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0);
        mDelegate.onSessionTileTouch(mNonDraggableTileView, nonDraggableUp);
        assertFalse(mDelegate.hasTileDragSession());
    }
}
