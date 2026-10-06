// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter.ViewHolder;

import java.time.Duration;
import java.util.List;

/** Unit tests for {@link TabListMergeAnimationManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabListMergeAnimationManagerUnitTest {
    /** Adapter with {@link #ITEM_COUNT} fixed-height items. */
    private static class TestAdapter extends RecyclerView.Adapter<ViewHolder> {
        @Override
        public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            View view = new View(parent.getContext());
            view.setLayoutParams(
                    new RecyclerView.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT, ITEM_HEIGHT));
            return new ViewHolder(view, (model, v, key) -> {});
        }

        @Override
        public void onBindViewHolder(ViewHolder holder, int position) {}

        @Override
        public int getItemCount() {
            return ITEM_COUNT;
        }
    }

    private static final int ITEM_HEIGHT = 100;
    private static final int ITEM_COUNT = 10;
    // Fully shows items 0 and 1, and only half of item 2.
    private static final int RECYCLER_VIEW_HEIGHT = 250;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mOnAnimationEndRunnable;
    @Mock private Runnable mSecondOnAnimationEndRunnable;

    private TabListRecyclerView mRecyclerView;
    private LinearLayoutManager mLayoutManager;
    private TabListMergeAnimationManager mAnimationManager;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout root = new FrameLayout(activity);
        activity.setContentView(root);

        mRecyclerView = new TabListRecyclerView(activity, /* attributeSet= */ null);
        mLayoutManager = spy(new LinearLayoutManager(activity));
        mRecyclerView.setLayoutManager(mLayoutManager);
        mRecyclerView.setAdapter(new TestAdapter());
        root.addView(
                mRecyclerView,
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, RECYCLER_VIEW_HEIGHT));
        RobolectricUtil.runAllBackgroundAndUi();

        mAnimationManager = new TabListMergeAnimationManager(mRecyclerView);
    }

    private View getItemView(int position) {
        RecyclerView.ViewHolder viewHolder =
                mRecyclerView.findViewHolderForAdapterPosition(position);
        return viewHolder.itemView;
    }

    private void runUiForMs(long ms) {
        shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(ms));
    }

    private void runAllAnimations() {
        runUiForMs(5000);
    }

    @Test
    public void testPlayAnimation_whenTargetIsVisible() {
        mAnimationManager.playAnimation(0, List.of(0, 1), mOnAnimationEndRunnable);

        assertTrue(mRecyclerView.isTouchInputBlockedForTesting());
        assertTrue(mRecyclerView.isSmoothScrollingForTesting());
        verify(mLayoutManager, never()).startSmoothScroll(any());
        verify(mOnAnimationEndRunnable, never()).run();

        runAllAnimations();

        assertFalse(mRecyclerView.isTouchInputBlockedForTesting());
        assertFalse(mRecyclerView.isSmoothScrollingForTesting());
        verify(mOnAnimationEndRunnable).run();
    }

    @Test
    public void testPlayAnimation_whenTargetIsNotVisible() {
        // Item 2 is only partially visible, so a scroll is needed before animating.
        mAnimationManager.playAnimation(2, List.of(1, 2), mOnAnimationEndRunnable);

        assertTrue(mRecyclerView.isTouchInputBlockedForTesting());
        assertTrue(mRecyclerView.isSmoothScrollingForTesting());
        verify(mLayoutManager).startSmoothScroll(any());
        verify(mOnAnimationEndRunnable, never()).run();

        // Let the smooth scroll settle and the merge animation run.
        runAllAnimations();

        assertFalse(mRecyclerView.isTouchInputBlockedForTesting());
        assertFalse(mRecyclerView.isSmoothScrollingForTesting());
        verify(mOnAnimationEndRunnable).run();
    }

    @Test
    public void testPlayAnimation_whenAlreadyAnimating() {
        mAnimationManager.playAnimation(0, List.of(0, 1), mOnAnimationEndRunnable);
        mAnimationManager.playAnimation(0, List.of(0, 1), mSecondOnAnimationEndRunnable);
        runAllAnimations();

        verify(mOnAnimationEndRunnable).run();
        verify(mSecondOnAnimationEndRunnable, never()).run();
    }

    @Test
    public void testAnimationCleanup() {
        View otherView = getItemView(1);

        mAnimationManager.playAnimation(0, List.of(1), mOnAnimationEndRunnable);
        runAllAnimations();

        // Alpha is not reset by cleanup, so this shows the merge animation actually ran.
        assertEquals(0f, otherView.getAlpha(), 0f);
        // Without cleanup, the other card would be left translated onto the target card.
        assertEquals(0f, otherView.getTranslationX(), 0f);
        assertEquals(0f, otherView.getTranslationY(), 0f);
        assertFalse(mRecyclerView.isTouchInputBlockedForTesting());
        assertFalse(mRecyclerView.isSmoothScrollingForTesting());
        verify(mOnAnimationEndRunnable).run();
    }

    @Test
    public void testPlayAnimation_nullTargetViewHolder() {
        // Without a layout manager there are no laid-out children, so no view holder is found.
        // This also skips the smooth scroll (covered by testPlayAnimation_whenTargetIsNotVisible).
        mRecyclerView.setLayoutManager(null);
        assertNull(mRecyclerView.findViewHolderForAdapterPosition(0));

        mAnimationManager.playAnimation(0, List.of(0, 1), mOnAnimationEndRunnable);
        runAllAnimations();

        assertFalse(mRecyclerView.isTouchInputBlockedForTesting());
        assertFalse(mRecyclerView.isSmoothScrollingForTesting());
        verify(mOnAnimationEndRunnable).run();
    }
}
