// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.bottom_sheet_utils;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;

/** Robolectric unit tests for {@link DetailScreenScrollListener}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class DetailScreenScrollListenerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mMockBottomSheetController;

    private DetailScreenScrollListener mScrollListener;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = Robolectric.setupActivity(android.app.Activity.class);
        mScrollListener = new DetailScreenScrollListener(mMockBottomSheetController);
    }

    /** Creates a test RecyclerView with deterministic scroll offset. */
    private RecyclerView createRecyclerViewWithOffset(int scrollOffset) {
        RecyclerView recyclerView =
                new RecyclerView(mContext) {
                    @Override
                    public int computeVerticalScrollOffset() {
                        return scrollOffset;
                    }
                };
        recyclerView.setLayoutManager(new LinearLayoutManager(mContext));
        return recyclerView;
    }

    /** Tests that listener starts in scrolled-to-top state. */
    @Test
    @SmallTest
    public void testInitialState() {
        assertTrue(mScrollListener.isScrolledToTop());
    }

    /** Tests that {@link DetailScreenScrollListener#reset} returns to scrolled-to-top state. */
    @Test
    @SmallTest
    public void testReset() {
        RecyclerView recyclerView = createRecyclerViewWithOffset(100);

        mScrollListener.onScrolled(recyclerView, 0, 10);
        assertFalse(mScrollListener.isScrolledToTop());

        mScrollListener.reset();
        assertTrue(mScrollListener.isScrolledToTop());
    }

    /**
     * Tests that scroll listener correctly identifies conditions that trigger layout suppression.
     */
    @Test
    @SmallTest
    public void testSuppressLayoutConditionsMet() {
        RecyclerView recyclerView = createRecyclerViewWithOffset(0);

        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.HALF);

        mScrollListener.onScrolled(recyclerView, 0, 0);

        assertTrue(mScrollListener.isScrolledToTop());
    }

    /** Tests that layout suppression is not triggered when RecyclerView is not at top position. */
    @Test
    @SmallTest
    public void testNoSuppressLayoutWhenNotAtTop() {
        RecyclerView recyclerView = createRecyclerViewWithOffset(100);

        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.HALF);

        mScrollListener.onScrolled(recyclerView, 0, 10);

        assertFalse(mScrollListener.isScrolledToTop());
    }

    /** Tests that layout suppression is not triggered when bottom sheet is not in half state. */
    @Test
    @SmallTest
    public void testNoSuppressLayoutWhenSheetNotHalf() {
        RecyclerView recyclerView = createRecyclerViewWithOffset(0);

        when(mMockBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);

        mScrollListener.onScrolled(recyclerView, 0, 0);

        assertTrue(mScrollListener.isScrolledToTop());
    }
}
