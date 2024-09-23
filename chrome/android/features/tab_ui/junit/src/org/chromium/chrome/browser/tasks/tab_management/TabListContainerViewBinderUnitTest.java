// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FETCH_VIEW_BY_INDEX_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FOCUS_TAB_INDEX_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.GET_VISIBLE_RANGE_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_SCROLLING_SUPPLIER_CALLBACK;

import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.NonNull;
import androidx.core.util.Function;
import androidx.core.util.Pair;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Robolectric tests for {@link TabListContainerViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
public class TabListContainerViewBinderUnitTest {
    private static class MockViewHolder extends RecyclerView.ViewHolder {
        public MockViewHolder(@NonNull View itemView) {
            super(itemView);
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabListRecyclerView mTabListRecyclerViewMock;
    @Mock private LinearLayoutManager mLinearLayoutManager;
    @Mock private View mViewMock1;
    @Mock private View mViewMock2;
    @Mock Callback<Function<Integer, View>> mFetchViewByIndexCallback;
    @Mock Callback<Supplier<Pair<Integer, Integer>>> mGetVisibleRangeCallback;
    @Mock Callback<ObservableSupplier<Boolean>> mIsScrollingSupplierCallback;

    @Captor ArgumentCaptor<Function<Integer, View>> mFetchViewByIndexCaptor;
    @Captor ArgumentCaptor<Supplier<Pair<Integer, Integer>>> mGetVisibleRangeCaptor;
    @Captor ArgumentCaptor<OnScrollListener> mOnScrollListenerCaptor;
    @Captor ArgumentCaptor<ObservableSupplier<Boolean>> mOnScrollingSupplierCaptor;

    @Before
    public void setUp() {
        when(mTabListRecyclerViewMock.getLayoutManager()).thenReturn(mLinearLayoutManager);
    }

    @Test
    @SmallTest
    public void testFocusTabIndexForAccessibilityProperty() {
        MockViewHolder viewHolder = spy(new MockViewHolder(mViewMock1));
        doReturn(viewHolder).when(mTabListRecyclerViewMock).findViewHolderForAdapterPosition(eq(2));
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabListContainerProperties.ALL_KEYS)
                        .with(FOCUS_TAB_INDEX_FOR_ACCESSIBILITY, 2)
                        .build();

        TabListContainerViewBinder.bind(
                propertyModel, mTabListRecyclerViewMock, FOCUS_TAB_INDEX_FOR_ACCESSIBILITY);

        verify(mViewMock1).requestFocus();
        verify(mViewMock1).sendAccessibilityEvent(eq(AccessibilityEvent.TYPE_VIEW_FOCUSED));
    }

    @Test
    @SmallTest
    public void testFetchViewByIndexCallback() {
        MockViewHolder viewHolder1 = spy(new MockViewHolder(mViewMock1));
        MockViewHolder viewHolder2 = spy(new MockViewHolder(mViewMock2));
        doReturn(viewHolder1).when(mTabListRecyclerViewMock).findViewHolderForAdapterPosition(0);
        doReturn(viewHolder2).when(mTabListRecyclerViewMock).findViewHolderForAdapterPosition(1);
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabListContainerProperties.ALL_KEYS)
                        .with(FETCH_VIEW_BY_INDEX_CALLBACK, mFetchViewByIndexCallback)
                        .build();

        TabListContainerViewBinder.bind(
                propertyModel, mTabListRecyclerViewMock, FETCH_VIEW_BY_INDEX_CALLBACK);

        verify(mFetchViewByIndexCallback).onResult(mFetchViewByIndexCaptor.capture());
        assertEquals(mViewMock1, mFetchViewByIndexCaptor.getValue().apply(0));
        assertEquals(mViewMock2, mFetchViewByIndexCaptor.getValue().apply(1));
        assertEquals(null, mFetchViewByIndexCaptor.getValue().apply(2));
    }

    @Test
    @SmallTest
    public void testGetVisibleRangeCallback() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabListContainerProperties.ALL_KEYS)
                        .with(GET_VISIBLE_RANGE_CALLBACK, mGetVisibleRangeCallback)
                        .build();

        TabListContainerViewBinder.bind(
                propertyModel, mTabListRecyclerViewMock, GET_VISIBLE_RANGE_CALLBACK);

        when(mLinearLayoutManager.findFirstCompletelyVisibleItemPosition()).thenReturn(1);
        when(mLinearLayoutManager.findLastCompletelyVisibleItemPosition()).thenReturn(2);
        verify(mGetVisibleRangeCallback).onResult(mGetVisibleRangeCaptor.capture());
        Pair<Integer, Integer> range = mGetVisibleRangeCaptor.getValue().get();
        assertNotNull(range);
        assertEquals(1, range.first.intValue());
        assertEquals(2, range.second.intValue());
    }

    @Test
    @SmallTest
    public void testIsScrollingSupplierCallback() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabListContainerProperties.ALL_KEYS)
                        .with(IS_SCROLLING_SUPPLIER_CALLBACK, mIsScrollingSupplierCallback)
                        .build();

        TabListContainerViewBinder.bind(
                propertyModel, mTabListRecyclerViewMock, IS_SCROLLING_SUPPLIER_CALLBACK);

        verify(mTabListRecyclerViewMock).addOnScrollListener(mOnScrollListenerCaptor.capture());
        OnScrollListener listener = mOnScrollListenerCaptor.getValue();
        verify(mIsScrollingSupplierCallback).onResult(mOnScrollingSupplierCaptor.capture());
        ObservableSupplier<Boolean> isScrollingSupplier = mOnScrollingSupplierCaptor.getValue();

        listener.onScrollStateChanged(mTabListRecyclerViewMock, RecyclerView.SCROLL_STATE_IDLE);
        assertFalse(isScrollingSupplier.get());

        listener.onScrollStateChanged(mTabListRecyclerViewMock, RecyclerView.SCROLL_STATE_DRAGGING);
        assertTrue(isScrollingSupplier.get());

        listener.onScrollStateChanged(mTabListRecyclerViewMock, RecyclerView.SCROLL_STATE_SETTLING);
        assertTrue(isScrollingSupplier.get());

        listener.onScrollStateChanged(mTabListRecyclerViewMock, RecyclerView.SCROLL_STATE_IDLE);
        assertFalse(isScrollingSupplier.get());
    }
}
