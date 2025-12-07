// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabStripSnapshotter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabStripSnapshotterTest {
    private static final PropertyKey[] PROPERTY_KEYS =
            new PropertyKey[] {
                TabProperties.FAVICON_FETCHER,
                TabProperties.FAVICON_FETCHED,
                TabProperties.IS_SELECTED
            };

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Captor private ArgumentCaptor<OnScrollListener> mOnScrollListenerCaptor;

    @Mock private RecyclerView mRecyclerView;
    @Mock private TabFaviconFetcher mTabFaviconFetcherA;
    @Mock private TabFaviconFetcher mTabFaviconFetcherB;
    @Mock private TabFaviconFetcher mTabFaviconFetcherC;

    private final List<Object> mTokenList = new ArrayList<>();

    private void onModelTokenChange(Object token) {
        mTokenList.add(token);
    }

    private static PropertyModel makePropertyModel(
            TabFaviconFetcher fetcher, boolean isSelected, boolean isFetched) {
        return new PropertyModel.Builder(PROPERTY_KEYS)
                .with(TabProperties.FAVICON_FETCHER, fetcher)
                .with(TabProperties.FAVICON_FETCHED, isFetched)
                .with(TabProperties.IS_SELECTED, isSelected)
                .build();
    }

    @Test
    public void testSnapshotterFetcher() {
        when(mRecyclerView.computeHorizontalScrollOffset()).thenReturn(0);
        ModelList modelList = new ModelList();
        PropertyModel propertyModel1 = makePropertyModel(mTabFaviconFetcherA, false, false);
        modelList.add(new ListItem(/* type= */ 0, propertyModel1));
        TabStripSnapshotter tabStripSnapshotter =
                new TabStripSnapshotter(this::onModelTokenChange, modelList, mRecyclerView);

        verify(mRecyclerView, times(1)).addOnScrollListener(mOnScrollListenerCaptor.capture());
        OnScrollListener onScrollListener = mOnScrollListenerCaptor.getValue();
        assertEquals(1, mTokenList.size());

        PropertyModel propertyModel2 = makePropertyModel(mTabFaviconFetcherA, true, true);
        modelList.add(new ListItem(/* type= */ 0, propertyModel2));
        assertEquals(2, mTokenList.size());
        assertNotEquals(mTokenList.get(0), mTokenList.get(1));

        propertyModel1.set(TabProperties.FAVICON_FETCHER, mTabFaviconFetcherC);
        assertEquals(3, mTokenList.size());
        assertNotEquals(mTokenList.get(1), mTokenList.get(2));

        propertyModel1.set(TabProperties.FAVICON_FETCHER, mTabFaviconFetcherA);
        assertEquals(4, mTokenList.size());
        assertNotEquals(mTokenList.get(2), mTokenList.get(3));

        propertyModel1.set(TabProperties.IS_SELECTED, true);
        assertEquals(5, mTokenList.size());
        assertNotEquals(mTokenList.get(3), mTokenList.get(4));

        propertyModel1.set(TabProperties.FAVICON_FETCHED, true);
        assertEquals(6, mTokenList.size());
        assertNotEquals(mTokenList.get(1), mTokenList.get(5));
        assertNotEquals(mTokenList.get(4), mTokenList.get(5));

        when(mRecyclerView.computeHorizontalScrollOffset()).thenReturn(100);
        onScrollListener.onScrollStateChanged(mRecyclerView, RecyclerView.SCROLL_STATE_DRAGGING);
        onScrollListener.onScrollStateChanged(mRecyclerView, RecyclerView.SCROLL_STATE_SETTLING);
        assertEquals(6, mTokenList.size());

        onScrollListener.onScrollStateChanged(mRecyclerView, RecyclerView.SCROLL_STATE_IDLE);
        assertEquals(7, mTokenList.size());
        assertNotEquals(mTokenList.get(5), mTokenList.get(6));

        when(mRecyclerView.computeHorizontalScrollOffset()).thenReturn(0);
        onScrollListener.onScrollStateChanged(mRecyclerView, RecyclerView.SCROLL_STATE_IDLE);
        assertEquals(8, mTokenList.size());
        assertEquals(mTokenList.get(5), mTokenList.get(7));

        tabStripSnapshotter.destroy();
        verify(mRecyclerView, times(1)).removeOnScrollListener(onScrollListener);
        propertyModel1.set(TabProperties.FAVICON_FETCHER, mTabFaviconFetcherB);
        assertEquals(8, mTokenList.size());
    }
}
