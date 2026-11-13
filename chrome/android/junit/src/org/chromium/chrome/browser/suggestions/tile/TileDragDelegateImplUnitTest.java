// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
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
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.TileDragDelegate.ReorderFlow;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link TileDragDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TileDragDelegateImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TileDragSession.EventListener mEventListener;

    private Activity mActivity;
    private MostVisitedTilesLayout mMvTilesLayout;
    private SuggestionsTileView mDraggableTileView1;
    private SuggestionsTileView mDraggableTileView2;
    private SuggestionsTileView mNonDraggableTileView;
    private TileDragDelegateImpl mDelegate;
    private int mTileWidth;

    private static class TestTileView extends SuggestionsTileView {
        private final SiteSuggestion mData;

        TestTileView(Context context, @TileSource.EnumType int source) {
            super(context, null);
            mData = new SiteSuggestion("title", new GURL("https://a.test"), 0, source, 0);
        }

        // Still needed: the real mData is only set by initialize(), which requires inflation.
        @Override
        public SiteSuggestion getData() {
            return mData;
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        HorizontalScrollView scrollView = new HorizontalScrollView(mActivity);
        mMvTilesLayout = new MostVisitedTilesLayout(mActivity, null);
        scrollView.addView(mMvTilesLayout);

        mTileWidth = mActivity.getResources().getDimensionPixelSize(R.dimen.tile_view_width);

        mDraggableTileView1 = new TestTileView(mActivity, TileSource.CUSTOM_LINKS);
        mDraggableTileView2 = new TestTileView(mActivity, TileSource.AI_MODE);
        mNonDraggableTileView = new TestTileView(mActivity, TileSource.TOP_SITES);

        mDraggableTileView1.layout(0, 0, mTileWidth, 100);
        mDraggableTileView2.layout(mTileWidth, 0, mTileWidth * 2, 100);
        mNonDraggableTileView.layout(mTileWidth * 2, 0, mTileWidth * 3, 100);

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
        ShadowLooper.idleMainLooper(TileDragDelegateImpl.START_DURATION_MS, TimeUnit.MILLISECONDS);
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

    // ==================== Drag Flow Tests ====================

    @Test
    public void testDragFlow_StartTimerTriggersDragStart() {
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mDraggableTileView1, downEvent, mEventListener);
        assertTrue(mDelegate.hasTileDragSession());

        // Prior to timer firing, onDragStart() has not been called.
        long preTriggerMs = TileDragDelegateImpl.START_DURATION_MS - 1;
        ShadowLooper.idleMainLooper(preTriggerMs, TimeUnit.MILLISECONDS);
        verify(mEventListener, never()).onDragStart();

        // Advance to START_DURATION_MS.
        ShadowLooper.idleMainLooper(
                TileDragDelegateImpl.START_DURATION_MS - preTriggerMs, TimeUnit.MILLISECONDS);
        verify(mEventListener).onDragStart();
        assertTrue(mDelegate.hasTileDragSession());
    }

    @Test
    public void testDragFlow_ReleaseBeforeTimerCancelsSession() {
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mDraggableTileView1, downEvent, mEventListener);
        assertTrue(mDelegate.hasTileDragSession());

        // User releases finger before timer fires.
        MotionEvent cancelEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_CANCEL, 0, 0, 0);
        mDelegate.onSessionTileTouch(mDraggableTileView1, cancelEvent);

        assertFalse(mDelegate.hasTileDragSession());
        ShadowLooper.idleMainLooper(TileDragDelegateImpl.START_DURATION_MS, TimeUnit.MILLISECONDS);
        verify(mEventListener, never()).onDragStart();
    }

    @Test
    public void testDragFlow_MoveBelowDominateThreshold_DoesNotTriggerDominate() {
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mDraggableTileView1, downEvent, mEventListener);
        ShadowLooper.idleMainLooper(TileDragDelegateImpl.START_DURATION_MS, TimeUnit.MILLISECONDS);

        // Move below DOMINATE_TRESHOLD_RATIO * tileWidth.
        float smallDx = mTileWidth * TileDragDelegateImpl.DOMINATE_TRESHOLD_RATIO * 0.5f;
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, smallDx, 0, 0);
        mDelegate.onSessionTileTouch(mDraggableTileView1, moveEvent);

        verify(mEventListener, never()).onDragDominate();
        assertTrue(mDelegate.hasTileDragSession());
    }

    @Test
    public void testDragFlow_MoveAboveDominateThreshold_TriggersDominate() {
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mDraggableTileView1, downEvent, mEventListener);
        ShadowLooper.idleMainLooper(TileDragDelegateImpl.START_DURATION_MS, TimeUnit.MILLISECONDS);

        // Move above DOMINATE_TRESHOLD_RATIO * tileWidth.
        float largeDx = mTileWidth * TileDragDelegateImpl.DOMINATE_TRESHOLD_RATIO + 1.0f;
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, largeDx, 0, 0);
        mDelegate.onSessionTileTouch(mDraggableTileView1, moveEvent);

        verify(mEventListener).onDragDominate();
    }

    @Test
    public void testDragFlow_DropInDominatePhase_DifferentTile_AcceptsReorder() {
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mDraggableTileView1, downEvent, mEventListener);
        ShadowLooper.idleMainLooper(TileDragDelegateImpl.START_DURATION_MS, TimeUnit.MILLISECONDS);

        // Drag to position of mDraggableTileView2 (distance = mTileWidth).
        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, mTileWidth, 0, 0);
        mDelegate.onSessionTileTouch(mDraggableTileView1, moveEvent);
        verify(mEventListener).onDragDominate();

        // Release finger.
        MotionEvent upEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, mTileWidth, 0, 0);
        mDelegate.onSessionTileTouch(mDraggableTileView1, upEvent);

        // Finish accept animation.
        ShadowLooper.idleMainLooper();

        verify(mEventListener)
                .onReorderAccept(
                        ReorderFlow.DRAG_FLOW,
                        mDraggableTileView1.getData(),
                        mDraggableTileView2.getData());
        verify(mEventListener, never()).onReorderCancel();
        assertFalse(mDelegate.hasTileDragSession());
    }

    @Test
    public void testDragFlow_DropInStartPhase_CancelsReorder() {
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mDraggableTileView1, downEvent, mEventListener);
        ShadowLooper.idleMainLooper(TileDragDelegateImpl.START_DURATION_MS, TimeUnit.MILLISECONDS);

        // Release immediately in START phase without exceeding dominate threshold.
        MotionEvent upEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0, 0, 0);
        mDelegate.onSessionTileTouch(mDraggableTileView1, upEvent);
        ShadowLooper.idleMainLooper();

        verify(mEventListener).onReorderCancel();
        verify(mEventListener, never()).onReorderAccept(anyInt(), any(), any());
    }

    @Test
    public void testDragFlow_DropInDominatePhase_ReturnedToStartTile_CancelsReorder() {
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mDraggableTileView1, downEvent, mEventListener);
        ShadowLooper.idleMainLooper(TileDragDelegateImpl.START_DURATION_MS, TimeUnit.MILLISECONDS);

        // Move past dominate threshold to view 2.
        MotionEvent moveEvent1 =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, mTileWidth, 0, 0);
        mDelegate.onSessionTileTouch(mDraggableTileView1, moveEvent1);
        verify(mEventListener).onDragDominate();

        // Move back to view 1's position (at coordinate 0). Since mFromView is now at
        // mTileWidth, the touch coordinate relative to mFromView is -mTileWidth.
        MotionEvent moveEvent2 =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, -mTileWidth, 0, 0);
        mDelegate.onSessionTileTouch(mDraggableTileView1, moveEvent2);

        // Release at original slot.
        MotionEvent upEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, -mTileWidth, 0, 0);
        mDelegate.onSessionTileTouch(mDraggableTileView1, upEvent);
        ShadowLooper.idleMainLooper();

        verify(mEventListener).onReorderCancel();
        verify(mEventListener, never()).onReorderAccept(anyInt(), any(), any());
    }

    @Test
    public void testDragFlow_ActionCancel_CancelsSession() {
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mDraggableTileView1, downEvent, mEventListener);
        ShadowLooper.idleMainLooper(TileDragDelegateImpl.START_DURATION_MS, TimeUnit.MILLISECONDS);

        MotionEvent cancelEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_CANCEL, 0, 0, 0);
        mDelegate.onSessionTileTouch(mDraggableTileView1, cancelEvent);
        ShadowLooper.idleMainLooper();

        assertFalse(mDelegate.hasTileDragSession());
        verify(mEventListener, never()).onReorderAccept(anyInt(), any(), any());
        // TODO(crbug.com/580907046): Call TileDragSession.finish() on ACTION_CANCEL in
        // onSessionTileTouch() so visuals reset and onReorderCancel() fires, then assert
        // scaleX == 1f, Z == 0f, and verify(mEventListener).onReorderCancel().
    }

    @Test
    public void testDragFlow_InterruptedDropAnimation_AbortsReorder() {
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mDraggableTileView1, downEvent, mEventListener);
        ShadowLooper.idleMainLooper(TileDragDelegateImpl.START_DURATION_MS, TimeUnit.MILLISECONDS);

        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, mTileWidth, 0, 0);
        mDelegate.onSessionTileTouch(mDraggableTileView1, moveEvent);

        // Drop tile (animation starts).
        MotionEvent upEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, mTileWidth, 0, 0);
        mDelegate.onSessionTileTouch(mDraggableTileView1, upEvent);

        // While animation is in-flight, user touches down again (soft reset).
        MotionEvent newDown = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mDraggableTileView1, newDown, mEventListener);

        // Complete all animations.
        ShadowLooper.idleMainLooper();

        // Drop reorder was aborted.
        verify(mEventListener, never()).onReorderAccept(eq(ReorderFlow.DRAG_FLOW), any(), any());
    }

    @Test
    public void testDragFlow_CancelActiveSession_AbortsReorder() {
        MotionEvent downEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mDelegate.onTileTouchDown(mDraggableTileView1, downEvent, mEventListener);
        ShadowLooper.idleMainLooper(TileDragDelegateImpl.START_DURATION_MS, TimeUnit.MILLISECONDS);

        MotionEvent moveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, mTileWidth, 0, 0);
        mDelegate.onSessionTileTouch(mDraggableTileView1, moveEvent);

        // Forcibly cancel active session.
        mDelegate.cancelActiveSession();
        ShadowLooper.idleMainLooper();

        assertFalse(mDelegate.hasTileDragSession());
        verify(mEventListener, never()).onReorderAccept(anyInt(), any(), any());
    }

    // ==================== Swap Flow Tests ====================

    @Test
    public void testSwapFlow_Forward_AcceptsReorder() {
        mDelegate.swapTiles(mDraggableTileView1, 1, mEventListener);
        ShadowLooper.idleMainLooper();

        verify(mEventListener)
                .onReorderAccept(
                        ReorderFlow.SWAP_FLOW,
                        mDraggableTileView1.getData(),
                        mDraggableTileView2.getData());
    }

    @Test
    public void testSwapFlow_Backward_AcceptsReorder() {
        mDelegate.swapTiles(mDraggableTileView2, -1, mEventListener);
        ShadowLooper.idleMainLooper();

        verify(mEventListener)
                .onReorderAccept(
                        ReorderFlow.SWAP_FLOW,
                        mDraggableTileView2.getData(),
                        mDraggableTileView1.getData());
    }

    @Test
    public void testSwapFlow_OutOfBounds_NoOps() {
        // Move left from index 0.
        mDelegate.swapTiles(mDraggableTileView1, -1, mEventListener);
        ShadowLooper.idleMainLooper();
        verify(mEventListener, never()).onReorderAccept(anyInt(), any(), any());

        // Move right from last draggable tile (index 1).
        mDelegate.swapTiles(mDraggableTileView2, 1, mEventListener);
        ShadowLooper.idleMainLooper();
        verify(mEventListener, never()).onReorderAccept(anyInt(), any(), any());
    }

    @Test
    public void testSwapFlow_NonDraggableTile_NoOps() {
        mDelegate.swapTiles(mNonDraggableTileView, 1, mEventListener);
        ShadowLooper.idleMainLooper();
        verify(mEventListener, never()).onReorderAccept(anyInt(), any(), any());
    }

    @Test
    public void testSwapFlow_FastChainedSwaps_FinalizesPreviousSwap() {
        // Swap 1 starts.
        mDelegate.swapTiles(mDraggableTileView1, 1, mEventListener);
        verify(mEventListener, never()).onReorderAccept(anyInt(), any(), any());

        // Before animation ends, Swap 2 is initiated. Swap 1 must be immediately finalized.
        mDelegate.swapTiles(mDraggableTileView2, -1, mEventListener);
        verify(mEventListener)
                .onReorderAccept(
                        ReorderFlow.SWAP_FLOW,
                        mDraggableTileView1.getData(),
                        mDraggableTileView2.getData());

        // Advance looper to finish Swap 2.
        ShadowLooper.idleMainLooper();
        verify(mEventListener)
                .onReorderAccept(
                        ReorderFlow.SWAP_FLOW,
                        mDraggableTileView2.getData(),
                        mDraggableTileView1.getData());
    }

    @Test
    public void testSwapFlow_CancelActiveSession_AbortsSwap() {
        mDelegate.swapTiles(mDraggableTileView1, 1, mEventListener);
        verify(mEventListener, never()).onReorderAccept(anyInt(), any(), any());

        // Cancel during animation.
        mDelegate.cancelActiveSession();
        ShadowLooper.idleMainLooper();

        // Swap must be aborted, not committed.
        verify(mEventListener, never()).onReorderAccept(anyInt(), any(), any());
        // TODO(crbug.com/580907046): Reset fromView Z in cancelActiveSession() for Swap Flow,
        // then assertEquals(0f, mDraggableTileView1.getZ(), 0f).
    }

    // ==================== Boundary Queries & Divider Tests ====================

    @Test
    public void testIsFirstAndLastDraggableTile() {
        assertTrue(mDelegate.isFirstDraggableTile(mDraggableTileView1));
        assertFalse(mDelegate.isFirstDraggableTile(mDraggableTileView2));
        assertFalse(mDelegate.isFirstDraggableTile(mNonDraggableTileView));

        assertFalse(mDelegate.isLastDraggableTile(mDraggableTileView1));
        assertTrue(mDelegate.isLastDraggableTile(mDraggableTileView2));
        assertFalse(mDelegate.isLastDraggableTile(mNonDraggableTileView));
    }

    @Test
    public void testDivider_ShowAndHide_SafeWhenNullOrPresent() {
        // Safe when no divider added.
        assertNull(mMvTilesLayout.getDividerMaybeNull());
        mDelegate.showDivider(/* isAnimated= */ false);
        mDelegate.hideDivider(/* isAnimated= */ false);

        // When divider added, showDivider() / hideDivider() update its alpha.
        SuggestionsTileVerticalDivider divider = new SuggestionsTileVerticalDivider(mActivity);
        mMvTilesLayout.addDivider(divider);
        assertEquals(0f, divider.getAlpha(), 0f);

        mDelegate.showDivider(/* isAnimated= */ false);
        assertEquals(1f, divider.getAlpha(), 0f);

        mDelegate.hideDivider(/* isAnimated= */ false);
        assertEquals(0f, divider.getAlpha(), 0f);
    }
}
