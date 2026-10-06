// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.ANIMATE_SUPPLEMENTARY_CONTAINER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FETCH_VIEW_BY_INDEX_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FOCUS_TAB_INDEX_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.GET_VISIBLE_RANGE_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.HUB_SEARCH_BOX_VISIBILITY_SUPPLIER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_SCROLLING_SUPPLIER_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.MANUAL_SEARCH_BOX_ANIMATION_SUPPLIER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.PAGE_KEY_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.SEARCH_BOX_VISIBILITY_FRACTION_SUPPLIER;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.core.util.Function;
import androidx.core.util.Pair;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.SupplementaryContainerAnimationMetadata;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** Robolectric tests for {@link TabListContainerViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabListContainerViewBinderUnitTest {
    /** Adapter with {@link #ITEM_COUNT} focusable plain views of a fixed height. */
    private static class TestAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
        @Override
        public @NonNull RecyclerView.ViewHolder onCreateViewHolder(
                @NonNull ViewGroup parent, int viewType) {
            View view = new View(parent.getContext());
            view.setLayoutParams(
                    new RecyclerView.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT, ITEM_HEIGHT_PX));
            view.setFocusable(true);
            return new RecyclerView.ViewHolder(view) {};
        }

        @Override
        public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position) {}

        @Override
        public int getItemCount() {
            return ITEM_COUNT;
        }
    }

    private static final int ITEM_COUNT = 5;
    private static final int ITEM_HEIGHT_PX = 100;
    private static final int RECYCLER_VIEW_SIZE_PX = 300;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Callback<Function<Integer, View>> mFetchViewByIndexCallback;
    @Mock Callback<Supplier<Pair<Integer, Integer>>> mGetVisibleRangeCallback;
    @Mock Callback<TabKeyEventData> mPageKeyEventDataCallback;
    @Mock View.AccessibilityDelegate mAccessibilityDelegate;

    @Captor ArgumentCaptor<Function<Integer, View>> mFetchViewByIndexCaptor;
    @Captor ArgumentCaptor<Supplier<Pair<Integer, Integer>>> mGetVisibleRangeCaptor;

    private TabListRecyclerView mTabListRecyclerView;
    private LinearLayout mSupplementaryContainer;
    private MonotonicObservableSupplier<Boolean> mIsScrollingSupplier;
    private TabListContainerViewBinder.ViewHolder mViewHolder;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mTabListRecyclerView = new TabListRecyclerView(activity, null);
        mTabListRecyclerView.setLayoutManager(new LinearLayoutManager(activity));
        mTabListRecyclerView.setAdapter(new TestAdapter());
        activity.setContentView(
                mTabListRecyclerView,
                new FrameLayout.LayoutParams(RECYCLER_VIEW_SIZE_PX, RECYCLER_VIEW_SIZE_PX));
        RobolectricUtil.runAllBackgroundAndUi();

        mSupplementaryContainer = new LinearLayout(activity);
        mViewHolder =
                new TabListContainerViewBinder.ViewHolder(
                        mTabListRecyclerView, new ImageView(activity), mSupplementaryContainer);
    }

    @Test
    public void testFocusTabIndexForAccessibilityProperty() {
        View itemView = mTabListRecyclerView.findViewHolderForAdapterPosition(2).itemView;
        itemView.setAccessibilityDelegate(mAccessibilityDelegate);
        assertFalse(itemView.isFocused());
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabListContainerProperties.ALL_KEYS)
                        .with(FOCUS_TAB_INDEX_FOR_ACCESSIBILITY, 2)
                        .build();

        TabListContainerViewBinder.bind(
                propertyModel, mViewHolder, FOCUS_TAB_INDEX_FOR_ACCESSIBILITY);

        assertTrue(itemView.isFocused());
        // Sent once by requestFocus() and once explicitly by the binder.
        verify(mAccessibilityDelegate, times(2))
                .sendAccessibilityEvent(itemView, AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    @Test
    public void testFetchViewByIndexCallback() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabListContainerProperties.ALL_KEYS)
                        .with(FETCH_VIEW_BY_INDEX_CALLBACK, mFetchViewByIndexCallback)
                        .build();

        TabListContainerViewBinder.bind(propertyModel, mViewHolder, FETCH_VIEW_BY_INDEX_CALLBACK);

        verify(mFetchViewByIndexCallback).onResult(mFetchViewByIndexCaptor.capture());
        Function<Integer, View> fetchViewByIndex = mFetchViewByIndexCaptor.getValue();
        assertEquals(
                mTabListRecyclerView.findViewHolderForAdapterPosition(0).itemView,
                fetchViewByIndex.apply(0));
        assertEquals(
                mTabListRecyclerView.findViewHolderForAdapterPosition(1).itemView,
                fetchViewByIndex.apply(1));
        // Only the first 3 items fit, so item 4 has no attached view.
        assertNull(fetchViewByIndex.apply(4));
    }

    @Test
    public void testGetVisibleRangeCallback() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabListContainerProperties.ALL_KEYS)
                        .with(GET_VISIBLE_RANGE_CALLBACK, mGetVisibleRangeCallback)
                        .build();

        TabListContainerViewBinder.bind(propertyModel, mViewHolder, GET_VISIBLE_RANGE_CALLBACK);

        verify(mGetVisibleRangeCallback).onResult(mGetVisibleRangeCaptor.capture());
        Supplier<Pair<Integer, Integer>> visibleRangeSupplier = mGetVisibleRangeCaptor.getValue();
        Pair<Integer, Integer> range = visibleRangeSupplier.get();
        assertNotNull(range);
        assertEquals(0, range.first.intValue());
        assertEquals(2, range.second.intValue());

        ((LinearLayoutManager) mTabListRecyclerView.getLayoutManager())
                .scrollToPositionWithOffset(1, 0);
        RobolectricUtil.runAllBackgroundAndUi();
        range = visibleRangeSupplier.get();
        assertEquals(1, range.first.intValue());
        assertEquals(3, range.second.intValue());
    }

    @Test
    public void testIsScrollingSupplierCallback() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabListContainerProperties.ALL_KEYS)
                        .with(
                                IS_SCROLLING_SUPPLIER_CALLBACK,
                                supplier -> mIsScrollingSupplier = supplier)
                        .build();
        TabListContainerViewBinder.bind(propertyModel, mViewHolder, IS_SCROLLING_SUPPLIER_CALLBACK);
        assertNotNull(mIsScrollingSupplier);
        assertFalse(mIsScrollingSupplier.get());

        mTabListRecyclerView.smoothScrollBy(0, ITEM_HEIGHT_PX);
        assertEquals(RecyclerView.SCROLL_STATE_SETTLING, mTabListRecyclerView.getScrollState());
        assertTrue(mIsScrollingSupplier.get());

        mTabListRecyclerView.stopScroll();
        assertEquals(RecyclerView.SCROLL_STATE_IDLE, mTabListRecyclerView.getScrollState());
        assertFalse(mIsScrollingSupplier.get());
    }

    @Test
    public void testPageKeyListenerCallback() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabListContainerProperties.ALL_KEYS)
                        .with(PAGE_KEY_LISTENER, mPageKeyEventDataCallback)
                        .build();

        TabListContainerViewBinder.bind(propertyModel, mViewHolder, PAGE_KEY_LISTENER);

        assertEquals(
                mPageKeyEventDataCallback,
                mTabListRecyclerView.getPageKeyListenerCallbackForTesting());
    }

    /**
     * Regression test: a burst of identical "show" requests during a fling must not force-finish
     * the in-flight animation, otherwise the search box would snap to its final position instead of
     * animating. Observable: the container's translationY stays at the animation's start value; a
     * force-finish would have snapped it to the end target.
     */
    @Test
    public void testAnimateSupplementaryContainer_burstOfIdenticalRequestsKeepsOneAnimation() {
        assertTrue(mViewHolder.mSearchBoxGapPx > 0);
        PropertyModel model = buildAnimationModel();

        for (int i = 0; i < 5; i++) {
            bindAnimate(model, /* shouldShowSearchBox= */ true);
        }

        assertTrue(mViewHolder.mSupplementaryContainerAnimationHandler.isAnimationPresent());
        assertEquals(0f, mSupplementaryContainer.getTranslationY(), 0.001f);

        mViewHolder.mSupplementaryContainerAnimationHandler.forceFinishAnimation();
        assertEquals(
                mViewHolder.mSearchBoxGapPx, mSupplementaryContainer.getTranslationY(), 0.001f);
    }

    /**
     * Regression test: when the user reverses swipe direction mid-fling, the new (different) target
     * must force-finish the in-flight animation and start a new one so the latest request wins.
     * Previously the reversal was silently dropped and the container stayed at the wrong
     * translation.
     */
    @Test
    public void testAnimateSupplementaryContainer_directionReversalForceFinishesAndStartsNew() {
        SettableNonNullObservableSupplier<Boolean> hubVisibilitySupplier =
                ObservableSuppliers.createNonNull(false);
        PropertyModel model = buildAnimationModel(hubVisibilitySupplier);

        bindAnimate(model, /* shouldShowSearchBox= */ true);
        assertTrue(hubVisibilitySupplier.get());

        bindAnimate(model, /* shouldShowSearchBox= */ false);
        assertTrue(mViewHolder.mSupplementaryContainerAnimationHandler.isAnimationPresent());

        // Finish the hide animation. If the hide had been dropped, no new animator would have
        // started after the show, and hubVisibilitySupplier would stay true.
        mViewHolder.mSupplementaryContainerAnimationHandler.forceFinishAnimation();
        assertFalse(hubVisibilitySupplier.get());
        assertEquals(0f, mSupplementaryContainer.getTranslationY(), 0.001f);
    }

    private PropertyModel buildAnimationModel() {
        return buildAnimationModel(ObservableSuppliers.createNonNull(false));
    }

    private PropertyModel buildAnimationModel(
            SettableNonNullObservableSupplier<Boolean> hubVisibilitySupplier) {
        return new PropertyModel.Builder(TabListContainerProperties.ALL_KEYS)
                .with(
                        MANUAL_SEARCH_BOX_ANIMATION_SUPPLIER,
                        ObservableSuppliers.createNonNull(false))
                .with(HUB_SEARCH_BOX_VISIBILITY_SUPPLIER, hubVisibilitySupplier)
                .with(
                        SEARCH_BOX_VISIBILITY_FRACTION_SUPPLIER,
                        ObservableSuppliers.createNonNull(0f))
                .build();
    }

    private void bindAnimate(PropertyModel model, boolean shouldShowSearchBox) {
        model.set(
                ANIMATE_SUPPLEMENTARY_CONTAINER,
                new SupplementaryContainerAnimationMetadata(
                        shouldShowSearchBox, /* forced= */ false));
        TabListContainerViewBinder.bind(model, mViewHolder, ANIMATE_SUPPLEMENTARY_CONTAINER);
    }
}
