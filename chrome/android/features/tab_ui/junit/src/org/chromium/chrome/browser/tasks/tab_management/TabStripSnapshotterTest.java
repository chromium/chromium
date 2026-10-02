// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
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
public class TabStripSnapshotterTest {
    private static final int ITEM_COUNT = 10;
    private static final int ITEM_WIDTH = 100;
    private static final int ITEM_HEIGHT = 10;
    private static final int VISIBLE_WIDTH = 3 * ITEM_WIDTH;
    private static final PropertyKey[] PROPERTY_KEYS =
            new PropertyKey[] {
                TabProperties.FAVICON_FETCHER,
                TabProperties.FAVICON_FETCHED,
                TabProperties.IS_SELECTED
            };

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabFaviconFetcher mTabFaviconFetcherA;
    @Mock private TabFaviconFetcher mTabFaviconFetcherB;
    @Mock private TabFaviconFetcher mTabFaviconFetcherC;

    private final List<Object> mTokenList = new ArrayList<>();
    private RecyclerView mRecyclerView;

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        mRecyclerView = new RecyclerView(context);
        mRecyclerView.setLayoutManager(
                new LinearLayoutManager(
                        context, LinearLayoutManager.HORIZONTAL, /* reverseLayout= */ false));
        mRecyclerView.setAdapter(new ItemAdapter());
        mRecyclerView.measure(
                View.MeasureSpec.makeMeasureSpec(VISIBLE_WIDTH, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(ITEM_HEIGHT, View.MeasureSpec.EXACTLY));
        mRecyclerView.layout(0, 0, VISIBLE_WIDTH, ITEM_HEIGHT);
    }

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

    /** Starts a fling (dispatching SCROLL_STATE_SETTLING) and then stops it (SCROLL_STATE_IDLE). */
    private void settleScroll() {
        // mRecyclerView is not attached to a window, so the fling's animation frames never run and
        // the scroll offset is left unchanged.
        mRecyclerView.smoothScrollBy(ITEM_WIDTH, 0);
        assertEquals(RecyclerView.SCROLL_STATE_SETTLING, mRecyclerView.getScrollState());
        mRecyclerView.stopScroll();
        assertEquals(RecyclerView.SCROLL_STATE_IDLE, mRecyclerView.getScrollState());
    }

    @Test
    public void testSnapshotterFetcher() {
        assertEquals(0, mRecyclerView.computeHorizontalScrollOffset());
        ModelList modelList = new ModelList();
        PropertyModel propertyModel1 = makePropertyModel(mTabFaviconFetcherA, false, false);
        modelList.add(new ListItem(/* type= */ 0, propertyModel1));
        TabStripSnapshotter tabStripSnapshotter =
                new TabStripSnapshotter(this::onModelTokenChange, modelList, mRecyclerView);

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

        mRecyclerView.scrollBy(ITEM_WIDTH, 0);
        assertEquals(ITEM_WIDTH, mRecyclerView.computeHorizontalScrollOffset());
        // Scrolling alone, or entering the settling state, doesn't take a snapshot.
        mRecyclerView.smoothScrollBy(ITEM_WIDTH, 0);
        assertEquals(6, mTokenList.size());

        mRecyclerView.stopScroll();
        assertEquals(7, mTokenList.size());
        assertNotEquals(mTokenList.get(5), mTokenList.get(6));

        mRecyclerView.scrollBy(-ITEM_WIDTH, 0);
        assertEquals(0, mRecyclerView.computeHorizontalScrollOffset());
        settleScroll();
        assertEquals(8, mTokenList.size());
        assertEquals(mTokenList.get(5), mTokenList.get(7));

        tabStripSnapshotter.destroy();
        // The scroll listener has been removed.
        settleScroll();
        assertEquals(8, mTokenList.size());
        propertyModel1.set(TabProperties.FAVICON_FETCHER, mTabFaviconFetcherB);
        assertEquals(8, mTokenList.size());
    }

    /** Creates {@link #ITEM_COUNT} fixed-width items. */
    private static class ItemAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            View view = new View(parent.getContext());
            view.setLayoutParams(
                    new RecyclerView.LayoutParams(ITEM_WIDTH, ViewGroup.LayoutParams.MATCH_PARENT));
            return new RecyclerView.ViewHolder(view) {};
        }

        @Override
        public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {}

        @Override
        public int getItemCount() {
            return ITEM_COUNT;
        }
    }
}
