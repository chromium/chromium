// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.MotionEvent;
import android.view.View;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BookmarkManagerDragHelper}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR_FAST_FOLLOW)
public class BookmarkManagerDragHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock SelectionDelegate<BookmarkId> mSelectionDelegate;
    @Mock ItemTouchHelper mItemTouchHelper;
    @Mock RecyclerView mRecyclerView;
    @Mock View mItemView;

    RecyclerView.ViewHolder mViewHolder;
    private Activity mActivity;
    private BookmarkManagerDragHelper mDragHelper;
    private BookmarkId mBookmarkId;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mBookmarkId = new BookmarkId(12345L, BookmarkType.NORMAL);
        mViewHolder = new RecyclerView.ViewHolder(mItemView) {};

        when(mItemView.getContext()).thenReturn(mActivity);

        mDragHelper =
                new BookmarkManagerDragHelper(
                        mActivity,
                        mBookmarkId,
                        mSelectionDelegate,
                        mItemTouchHelper,
                        mRecyclerView,
                        mViewHolder,
                        /* isDragEnabled= */ true);
    }

    @Test
    public void testTouch_Scenario1_LongPressSelectsAndDrags() {
        // Scenario 1: None of the items in the list are selected.
        doReturn(false).when(mSelectionDelegate).isItemSelected(mBookmarkId);
        doReturn(false).when(mSelectionDelegate).isSelectionEnabled();

        // Perform action down.
        boolean consumed =
                mDragHelper.onRowBodyTouch(mItemView, obtainEvent(MotionEvent.ACTION_DOWN));
        assertTrue(consumed);

        // Checkpoint 1: Advance time to 499ms, just before selection.
        ShadowLooper.idleMainLooper(499, TimeUnit.MILLISECONDS);
        // Verify that the selection has not occurred yet.
        verify(mSelectionDelegate, never()).toggleSelectionForItem(any());

        // Checkpoint 2: Advance time by another 100ms (Crossing the 500ms threshold).
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        // Selection should be triggered now.
        verify(mSelectionDelegate).toggleSelectionForItem(mBookmarkId);

        // Verify drag hasn't started yet here (599ms).
        verify(mItemTouchHelper, never()).startDrag(any());

        // Checkpoint 3: Advance another 100ms to fire the second runnable. Now at 699ms.
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        // Dragging should have started.
        verify(mItemTouchHelper).startDrag(mViewHolder);
    }

    @Test
    public void testTouch_Scenario2_AlreadySelectedStartsDragQuickly() {
        // Scenario 2: Selection Active, item is already selected.
        doReturn(true).when(mSelectionDelegate).isItemSelected(mBookmarkId);
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();

        // Perform action down.
        mDragHelper.onRowBodyTouch(mItemView, obtainEvent(MotionEvent.ACTION_DOWN));

        // Verify drag starts quickly (100ms) without toggling selection.
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        verify(mItemTouchHelper).startDrag(mViewHolder);
        verify(mSelectionDelegate, never()).toggleSelectionForItem(any());
    }

    @Test
    public void testTouch_Scenario3_SelectionActiveButNotThisItem() {
        // Scenario 3: Selection Active (other items are selected), but this item is not.
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();
        doReturn(false).when(mSelectionDelegate).isItemSelected(mBookmarkId);

        // Perform action down.
        mDragHelper.onRowBodyTouch(mItemView, obtainEvent(MotionEvent.ACTION_DOWN));

        // Verify that we long-press for 500ms. Drag should not have started at 499ms.
        ShadowLooper.idleMainLooper(499, TimeUnit.MILLISECONDS);
        verify(mItemTouchHelper, never()).startDrag(any());

        // Advance to 599ms.
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);

        // Verify that drag has started.
        verify(mItemTouchHelper).startDrag(mViewHolder);
        // Verify that selection toggle has never happened.
        verify(mSelectionDelegate, never()).toggleSelectionForItem(any());
    }

    @Test
    public void test_onViewDetachedFromWindow() {
        // Perform action down.
        mDragHelper.onRowBodyTouch(mItemView, obtainEvent(MotionEvent.ACTION_DOWN));

        // Simulate the recyclerView recycling the view (ImprovedBookmarkRow gets detached).
        mDragHelper.onViewDetachedFromWindow(mItemView);

        // Fast forward time significantly.
        ShadowLooper.idleMainLooper(10000, TimeUnit.MILLISECONDS);

        // Verify that the timer has died and that selection and drag have not happened.
        verify(mSelectionDelegate, never()).toggleSelectionForItem(any());
        verify(mItemTouchHelper, never()).startDrag(any());

        // Verify that the we cleaned up the listener.
        verify(mItemView).removeOnAttachStateChangeListener(mDragHelper);
    }

    // Obtain the action event we want to perform (ACTION_DOWN, etc.).
    private MotionEvent obtainEvent(int action) {
        // Use coordinates that are definitely not (0,0) and likely not where the drag handle is.
        long downTime = android.os.SystemClock.uptimeMillis();
        return MotionEvent.obtain(
                /* downTime= */ downTime,
                /* eventTime= */ downTime,
                /* action= */ action,
                /* x= */ 50f,
                /* y= */ 50f,
                /* metaState= */ 0);
    }
}
