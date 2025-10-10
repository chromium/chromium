// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.Collections;
import java.util.List;
import java.util.function.IntConsumer;
import java.util.function.IntUnaryOperator;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

/** Unit tests for {@link BookmarkBarItemsLayoutManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarItemsLayoutManagerTest {

    private static final WritableIntPropertyKey HEIGHT = new WritableIntPropertyKey();
    private static final WritableIntPropertyKey WIDTH = new WritableIntPropertyKey();
    private static final PropertyKey[] ALL_KEYS = new PropertyKey[] {HEIGHT, WIDTH};

    private static final int VIEW_TYPE = 1;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<Boolean> mItemsOverflowSupplierObserver;

    private SimpleRecyclerViewAdapter mAdapter;
    private int mItemMinWidth;
    private int mItemMaxWidth;
    private int mItemSpacing;
    private BookmarkBarItemsLayoutManager mLayoutManager;
    private ModelList mModel;
    private RecyclerView mView;

    @Before
    public void setUp() {
        mModel = new ModelList();
        mAdapter = new SimpleRecyclerViewAdapter(mModel);
        mAdapter.registerType(VIEW_TYPE, this::buildItemView, this::bindItemView);

        final var context = ApplicationProvider.getApplicationContext();
        final var res = context.getResources();
        mItemMinWidth = res.getDimensionPixelSize(R.dimen.bookmark_bar_item_min_width);
        mItemMaxWidth = res.getDimensionPixelSize(R.dimen.bookmark_bar_item_max_width);
        mItemSpacing = res.getDimensionPixelSize(R.dimen.bookmark_bar_item_spacing);

        mLayoutManager = new BookmarkBarItemsLayoutManager(context);
        mLayoutManager.setItemWidthConstraints(mItemMinWidth, mItemMaxWidth);

        mView = new RecyclerView(context);
        mView.setAdapter(mAdapter);
        mView.setLayoutManager(mLayoutManager);
    }

    @After
    public void tearDown() {
        // Reset forced desiredWidths.
        mLayoutManager.setDesiredWidthsForTesting(null);
    }

    private void bindItemView(
            @NonNull PropertyModel model, @NonNull View itemView, @NonNull PropertyKey key) {
        if (key == HEIGHT) {
            final var lp = itemView.getLayoutParams();
            lp.height = model.get(HEIGHT);
            itemView.setLayoutParams(lp);
        } else if (key == WIDTH) {
            final var lp = itemView.getLayoutParams();
            lp.width = model.get(WIDTH);
            itemView.setLayoutParams(lp);
        }
    }

    private @NonNull List<ListItem> buildItemList(
            @NonNull List<Integer> itemWidths, int itemHeight) {
        // When making a new item list, we need to manually set the desiredWidths in the
        // BookmarkBarItemsLayoutManager because the measured width in tests is always 0. Tests can
        // pass on all zeros with luck, without actually testing anything meaningful.
        int[] widths = new int[itemWidths.size()];
        for (int i = 0; i < itemWidths.size(); i++) {
            widths[i] = itemWidths.get(i);
        }
        mLayoutManager.setDesiredWidthsForTesting(widths);

        return itemWidths.stream()
                .map(itemWidth -> buildListItem(itemWidth, itemHeight))
                .collect(Collectors.toList());
    }

    private @NonNull View buildItemView(@NonNull ViewGroup parent) {
        final var itemView = new View(parent.getContext());
        itemView.setLayoutParams(new ViewGroup.LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        return itemView;
    }

    private @NonNull ListItem buildListItem(int itemWidth, int itemHeight) {
        return new ListItem(
                VIEW_TYPE,
                new PropertyModel.Builder(ALL_KEYS)
                        .with(HEIGHT, itemHeight)
                        .with(WIDTH, itemWidth)
                        .build());
    }

    private int calculateLayoutWidth(@NonNull List<Integer> itemWidths) {
        return itemWidths.stream().mapToInt(Integer::intValue).sum()
                + Math.max(0, mItemSpacing * (itemWidths.size() - 1));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR_FAST_FOLLOW)
    public void testLayout_shrinksToFit() {
        // Set up items that will overflow at max width, but fit at min width.
        final var itemHeight = 10;
        final var itemWidths = Collections.nCopies(10, mItemMaxWidth);
        mModel.set(buildItemList(itemWidths, itemHeight));

        // Set the total width to be between the min and max width.
        int totalWidthAtMax = calculateLayoutWidth(itemWidths);
        int totalWidthAtMin =
                itemWidths.size() * mItemMinWidth + (mItemSpacing * (itemWidths.size() - 1));
        int layoutWidth = (totalWidthAtMax + totalWidthAtMin) / 2;

        mView.layout(0, 0, layoutWidth, itemHeight);

        // All items should be visible.
        assertEquals(itemWidths.size(), mView.getChildCount());

        // The effective width should be between min and max.
        int effectiveItemWidth = mView.getChildAt(0).getWidth();
        assertTrue("Items should be more than the min width.", effectiveItemWidth > mItemMinWidth);
        assertTrue("Items should be less than the max width.", effectiveItemWidth < mItemMaxWidth);
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR_FAST_FOLLOW)
    public void testLayout_overflows() {
        mLayoutManager.getItemsOverflowSupplier().addObserver(mItemsOverflowSupplierObserver);
        Robolectric.flushForegroundThreadScheduler();
        clearInvocations(mItemsOverflowSupplierObserver);

        // Set up items that will overflow even at min width.
        final var itemHeight = 10;
        final var itemWidths = Collections.nCopies(20, mItemMaxWidth);
        mModel.set(buildItemList(itemWidths, itemHeight));

        // Set the layout width to be smaller than the minimum required width. We make it relatively
        // small because there is a chance to have the widths and paddings add up to the point that
        // X fit exactly into the layout. We can guarantee this doesn't happen by making the width
        // equal to two views, plus one padding, plus half the size of a third view.
        int layoutWidth =
                itemWidths.get(0) + mItemSpacing + itemWidths.get(1) + (itemWidths.get(2) / 2);

        mView.layout(0, 0, layoutWidth, itemHeight);

        // Verify that not all items are visible and the overflow supplier is notified.
        assertTrue(
                "Not all items should be visible and overflow should be present.",
                mView.getChildCount() < itemWidths.size());
        verify(mItemsOverflowSupplierObserver).onResult(true);

        // The visible items should be wider than the minimum, since they expand to fill space.
        assertTrue(
                "Items should be more than the minimum to fill up space.",
                mView.getChildAt(0).getWidth() > mItemMinWidth);
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR_FAST_FOLLOW)
    public void testLayout_fitsAtMaxWidth() {
        // Set up a few items.
        final var itemHeight = 10;
        final var itemWidths = Collections.nCopies(5, mItemMaxWidth);
        mModel.set(buildItemList(itemWidths, itemHeight));

        // Provide more than enough space.
        int totalWidthAtMax = calculateLayoutWidth(itemWidths);
        mView.layout(0, 0, totalWidthAtMax + 100, itemHeight);

        // All items should be visible.
        assertEquals(itemWidths.size(), mView.getChildCount());

        // The effective width should be exactly the max width.
        for (int i = 0; i < mView.getChildCount(); i++) {
            assertEquals(mItemMaxWidth, mView.getChildAt(i).getWidth());
        }
    }

    @Test
    @SmallTest
    public void testItemsOverflowChangeCallback() {
        // Bind observer and verify initial event propagation.
        verify(mItemsOverflowSupplierObserver, never()).onResult(any());
        mLayoutManager.getItemsOverflowSupplier().addObserver(mItemsOverflowSupplierObserver);
        Robolectric.flushForegroundThreadScheduler();
        verify(mItemsOverflowSupplierObserver).onResult(false);
        clearInvocations(mItemsOverflowSupplierObserver);

        // Set up items.
        final var itemHeight = 10;
        final var itemWidth = 10;
        final var itemWidths = Collections.nCopies(10, itemWidth);
        mModel.set(buildItemList(itemWidths, itemHeight));

        // Perform layout of view w/ less space than is needed and verify event propagation.
        mView.layout(0, 0, calculateLayoutWidth(itemWidths) - 1, itemHeight);
        verify(mItemsOverflowSupplierObserver).onResult(true);
        clearInvocations(mItemsOverflowSupplierObserver);

        // Verify re-layout is a no-op.
        mView.layout(mView.getLeft(), mView.getTop(), mView.getRight(), mView.getBottom());
        verify(mItemsOverflowSupplierObserver, never()).onResult(any());

        // Perform layout of view w/ as much space as is needed and verify event propagation.
        mView.layout(0, 0, calculateLayoutWidth(itemWidths), itemHeight);
        verify(mItemsOverflowSupplierObserver).onResult(false);
        clearInvocations(mItemsOverflowSupplierObserver);

        // Verify re-layout is a no-op.
        mView.layout(mView.getLeft(), mView.getTop(), mView.getRight(), mView.getBottom());
        verify(mItemsOverflowSupplierObserver, never()).onResult(any());

        // Clean up.
        mLayoutManager.getItemsOverflowSupplier().removeObserver(mItemsOverflowSupplierObserver);
    }

    @Test
    @SmallTest
    public void testLayout() {
        testLayout(/* isLayoutRtl= */ false);
    }

    @Test
    @SmallTest
    public void testLayoutInRtl() {
        testLayout(/* isLayoutRtl= */ true);
    }

    private void testLayout(boolean isLayoutRtl) {
        // Set up layout direction.
        LocalizationUtils.setRtlForTesting(isLayoutRtl);

        // Set up items of varying widths.
        final var itemHeight = 10;
        final var itemWidths = List.of(10, 20, 30, 40, 50);
        mModel.set(buildItemList(itemWidths, itemHeight));

        // Cache a function which returns expected start position for item index.
        final IntUnaryOperator getItemStartForIndex =
                (i) -> {
                    int start = IntStream.range(0, i).map(itemWidths::get).sum() + mItemSpacing * i;
                    return isLayoutRtl ? mView.getWidth() - start : start;
                };

        // Cache a function to validate expected layout.
        final IntConsumer assertLayout =
                (expectedChildCount) -> {
                    assertEquals(expectedChildCount, mView.getChildCount());
                    for (int i = 0; i < expectedChildCount; i++) {
                        // Assert model item order matches item view order.
                        final var itemView = mView.getChildAt(i);
                        assertEquals(i, mLayoutManager.getPosition(itemView));

                        // Assert item view position.
                        int itemViewStart = isLayoutRtl ? itemView.getRight() : itemView.getLeft();
                        assertEquals(getItemStartForIndex.applyAsInt(i), itemViewStart);
                        assertEquals(0, itemView.getTop());

                        // Assert item view size.
                        final int itemWidth = itemWidths.get(i);
                        assertEquals(itemWidth, itemView.getWidth());
                        assertEquals(itemHeight, itemView.getHeight());
                    }
                };

        // Perform and validate layout of view w/ more space than is needed.
        mView.layout(0, 0, calculateLayoutWidth(itemWidths) + 1, itemHeight);
        assertLayout.accept(/* expectedChildCount= */ itemWidths.size());

        // Perform and validate layout of view w/ exactly as much space as is needed.
        mView.layout(0, 0, calculateLayoutWidth(itemWidths), itemHeight);
        assertLayout.accept(/* expectedChildCount= */ itemWidths.size());

        // Perform and validate layout of view w/ less space than is needed.
        mView.layout(0, 0, calculateLayoutWidth(itemWidths) - 1, itemHeight);
        assertLayout.accept(/* expectedChildCount= */ itemWidths.size() - 1);
    }

    @Test
    @SmallTest
    public void testMaxWidth() {
        // Set up items of widths less than, equal to, and greater than constraints.
        final var itemHeight = 10;
        final var itemWidths = List.of(mItemMaxWidth - 1, mItemMaxWidth, mItemMaxWidth + 1);
        mModel.set(buildItemList(itemWidths, itemHeight));

        // Perform layout of view and validate widths do not exceed constraints.
        mView.layout(0, 0, calculateLayoutWidth(itemWidths), itemHeight);
        assertEquals(itemWidths.size(), mView.getChildCount());
        for (int i = 0; i < itemWidths.size(); i++) {
            final var itemView = mView.getChildAt(i);
            final int itemWidth = Math.min(itemWidths.get(i), mItemMaxWidth);
            assertEquals(itemWidth, itemView.getWidth());
        }

        // Update constraints.
        mItemMaxWidth -= 1;
        mLayoutManager.setItemWidthConstraints(mItemMinWidth, mItemMaxWidth);

        // Perform measure/layout of view and validate widths do not exceed constraints.
        mView.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        mView.layout(0, 0, calculateLayoutWidth(itemWidths), itemHeight);
        assertEquals(itemWidths.size(), mView.getChildCount());
        for (int i = 0; i < itemWidths.size(); i++) {
            final var itemView = mView.getChildAt(i);
            final int itemWidth = Math.min(itemWidths.get(i), mItemMaxWidth);
            assertEquals(itemWidth, itemView.getWidth());
        }
    }

    @Test
    @SmallTest
    public void testNonScrollability() {
        assertFalse(mLayoutManager.canScrollHorizontally());
        assertFalse(mLayoutManager.canScrollVertically());
    }
}
