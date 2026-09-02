// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.ANIMATE_SUPPLEMENTARY_CONTAINER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FETCH_VIEW_BY_INDEX_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FOCUS_TAB_INDEX_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.GET_VISIBLE_RANGE_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.HUB_SEARCH_BOX_VISIBILITY_SUPPLIER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_SCROLLING_SUPPLIER_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.MANUAL_SEARCH_BOX_ANIMATION_SUPPLIER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.PAGE_KEY_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.SEARCH_BOX_VISIBILITY_FRACTION_SUPPLIER;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.core.util.Function;
import androidx.core.util.Pair;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.SupplementaryContainerAnimationMetadata;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** Robolectric tests for {@link TabListContainerViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabListContainerViewBinderUnitTest {
    private static class MockViewHolder extends RecyclerView.ViewHolder {
        public MockViewHolder(@NonNull View itemView) {
            super(itemView);
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabListRecyclerView mTabListRecyclerViewMock;
    @Mock private LinearLayoutManager mLinearLayoutManager;
    @Mock private ImageView mPaneHairlineMock;
    @Mock private LinearLayout mSupplementaryContainerMock;
    @Mock private View mViewMock1;
    @Mock private View mViewMock2;
    @Mock private Context mContextMock;
    @Mock private Resources mResourcesMock;
    @Mock Callback<Function<Integer, View>> mFetchViewByIndexCallback;
    @Mock Callback<Supplier<Pair<Integer, Integer>>> mGetVisibleRangeCallback;
    @Mock Callback<TabKeyEventData> mPageKeyEventDataCallback;

    @Captor ArgumentCaptor<Function<Integer, View>> mFetchViewByIndexCaptor;
    @Captor ArgumentCaptor<Supplier<Pair<Integer, Integer>>> mGetVisibleRangeCaptor;
    @Captor ArgumentCaptor<OnScrollListener> mOnScrollListenerCaptor;

    private MonotonicObservableSupplier<Boolean> mIsScrollingSupplier;
    private TabListContainerViewBinder.ViewHolder mViewHolder;
    private float mSupplementaryContainerTranslationY;

    @Before
    public void setUp() {
        when(mTabListRecyclerViewMock.findViewById(R.id.tab_list_recycler_view))
                .thenReturn(mTabListRecyclerViewMock);
        when(mTabListRecyclerViewMock.getLayoutManager()).thenReturn(mLinearLayoutManager);
        when(mTabListRecyclerViewMock.getResources()).thenReturn(mResourcesMock);
        when(mTabListRecyclerViewMock.getContext()).thenReturn(mContextMock);
        when(mContextMock.getResources()).thenReturn(mResourcesMock);
        when(mResourcesMock.getDimensionPixelSize(R.dimen.hub_search_box_gap)).thenReturn(10);

        // Round-trip translationY on the mock so getTranslationY() reflects the latest
        // setTranslationY() call. The bind logic reads translationY back after force-finish, and
        // an early-return optimization depends on it being non-zero post-finish.
        mSupplementaryContainerTranslationY = 0f;
        when(mSupplementaryContainerMock.getTranslationY())
                .thenAnswer(invocation -> mSupplementaryContainerTranslationY);
        doAnswer(
                        invocation -> {
                            mSupplementaryContainerTranslationY = invocation.getArgument(0);
                            return null;
                        })
                .when(mSupplementaryContainerMock)
                .setTranslationY(anyFloat());

        mViewHolder =
                new TabListContainerViewBinder.ViewHolder(
                        mTabListRecyclerViewMock, mPaneHairlineMock, mSupplementaryContainerMock);
    }

    @Test
    public void testFocusTabIndexForAccessibilityProperty() {
        MockViewHolder viewHolder = spy(new MockViewHolder(mViewMock1));
        doReturn(viewHolder).when(mTabListRecyclerViewMock).findViewHolderForAdapterPosition(eq(2));
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabListContainerProperties.ALL_KEYS)
                        .with(FOCUS_TAB_INDEX_FOR_ACCESSIBILITY, 2)
                        .build();

        TabListContainerViewBinder.bind(
                propertyModel, mViewHolder, FOCUS_TAB_INDEX_FOR_ACCESSIBILITY);

        verify(mViewMock1).requestFocus();
        verify(mViewMock1).sendAccessibilityEvent(eq(AccessibilityEvent.TYPE_VIEW_FOCUSED));
    }

    @Test
    public void testFetchViewByIndexCallback() {
        MockViewHolder viewHolder1 = spy(new MockViewHolder(mViewMock1));
        MockViewHolder viewHolder2 = spy(new MockViewHolder(mViewMock2));
        doReturn(viewHolder1).when(mTabListRecyclerViewMock).findViewHolderForAdapterPosition(0);
        doReturn(viewHolder2).when(mTabListRecyclerViewMock).findViewHolderForAdapterPosition(1);
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabListContainerProperties.ALL_KEYS)
                        .with(FETCH_VIEW_BY_INDEX_CALLBACK, mFetchViewByIndexCallback)
                        .build();

        TabListContainerViewBinder.bind(propertyModel, mViewHolder, FETCH_VIEW_BY_INDEX_CALLBACK);

        verify(mFetchViewByIndexCallback).onResult(mFetchViewByIndexCaptor.capture());
        assertEquals(mViewMock1, mFetchViewByIndexCaptor.getValue().apply(0));
        assertEquals(mViewMock2, mFetchViewByIndexCaptor.getValue().apply(1));
        assertEquals(null, mFetchViewByIndexCaptor.getValue().apply(2));
    }

    @Test
    public void testGetVisibleRangeCallback() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabListContainerProperties.ALL_KEYS)
                        .with(GET_VISIBLE_RANGE_CALLBACK, mGetVisibleRangeCallback)
                        .build();

        TabListContainerViewBinder.bind(propertyModel, mViewHolder, GET_VISIBLE_RANGE_CALLBACK);

        when(mLinearLayoutManager.findFirstCompletelyVisibleItemPosition()).thenReturn(1);
        when(mLinearLayoutManager.findLastCompletelyVisibleItemPosition()).thenReturn(2);
        verify(mGetVisibleRangeCallback).onResult(mGetVisibleRangeCaptor.capture());
        Pair<Integer, Integer> range = mGetVisibleRangeCaptor.getValue().get();
        assertNotNull(range);
        assertEquals(1, range.first.intValue());
        assertEquals(2, range.second.intValue());
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

        verify(mTabListRecyclerViewMock).addOnScrollListener(mOnScrollListenerCaptor.capture());
        OnScrollListener listener = mOnScrollListenerCaptor.getValue();
        assertNotNull(mIsScrollingSupplier);

        listener.onScrollStateChanged(mTabListRecyclerViewMock, RecyclerView.SCROLL_STATE_IDLE);
        assertFalse(mIsScrollingSupplier.get());

        listener.onScrollStateChanged(mTabListRecyclerViewMock, RecyclerView.SCROLL_STATE_DRAGGING);
        assertTrue(mIsScrollingSupplier.get());

        listener.onScrollStateChanged(mTabListRecyclerViewMock, RecyclerView.SCROLL_STATE_SETTLING);
        assertTrue(mIsScrollingSupplier.get());

        listener.onScrollStateChanged(mTabListRecyclerViewMock, RecyclerView.SCROLL_STATE_IDLE);
        assertFalse(mIsScrollingSupplier.get());
    }

    @Test
    public void testPageKeyListenerCallback() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabListContainerProperties.ALL_KEYS)
                        .with(PAGE_KEY_LISTENER, mPageKeyEventDataCallback)
                        .build();

        TabListContainerViewBinder.bind(propertyModel, mViewHolder, PAGE_KEY_LISTENER);

        verify(mTabListRecyclerViewMock, times(1))
                .setPageKeyListenerCallback(mPageKeyEventDataCallback);
    }

    /**
     * Regression test: a burst of identical "show" requests during a fling must not force-finish
     * the in-flight animation, otherwise the search box would snap to its final position instead of
     * animating. Observable: {@link android.view.View#setTranslationY} is called exactly once (from
     * the start-value seed of the first animator); any further calls indicate a force-finish that
     * snapped the value to the end target.
     */
    @Test
    public void testAnimateSupplementaryContainer_burstOfIdenticalRequestsKeepsOneAnimation() {
        PropertyModel model = buildAnimationModel();

        for (int i = 0; i < 5; i++) {
            bindAnimate(model, /* shouldShowSearchBox= */ true);
        }

        assertTrue(mViewHolder.mSupplementaryContainerAnimationHandler.isAnimationPresent());
        verify(mSupplementaryContainerMock, times(1)).setTranslationY(anyFloat());
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
        assertEquals(0f, mSupplementaryContainerTranslationY, 0.001f);
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
