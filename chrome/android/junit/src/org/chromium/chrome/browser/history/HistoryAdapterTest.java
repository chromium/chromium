// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.chromium.chrome.browser.history.HistoryTestUtils.checkAdapterContents;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.MoreProgressButton;

import java.util.Date;
import java.util.concurrent.TimeUnit;

/** Tests for the {@link HistoryAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HistoryAdapterTest {
    private StubbedHistoryProvider mHistoryProvider;
    private HistoryAdapter mAdapter;

    @Mock private MoreProgressButton mMockButton;
    @Mock private HistoryContentManager mContentManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mHistoryProvider = new StubbedHistoryProvider();
        mAdapter =
                new HistoryAdapter(
                        mContentManager,
                        mHistoryProvider,
                        new ObservableSupplierImpl<>(),
                        (vg) -> null);
        mAdapter.generateHeaderItemsForTest();
        mAdapter.generateFooterItemsForTest(mMockButton);
    }

    private void initializeAdapter() {
        mAdapter.startLoadingItems();
    }

    @Test
    public void testInitialize_Empty() {
        mAdapter.startLoadingItems();
        checkAdapterContents(mAdapter, false, false);
    }

    @Test
    public void testInitialize_SingleItem() {
        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mHistoryProvider.addItem(item1);

        mAdapter.startLoadingItems();

        // There should be three items - the header, a date and the history item.
        checkAdapterContents(mAdapter, true, false, null, null, item1);
    }

    @Test
    public void testRemove_TwoItemsOneDate() {
        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mHistoryProvider.addItem(item1);

        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, timestamp);
        mHistoryProvider.addItem(item2);

        mAdapter.startLoadingItems();

        // There should be four items - the list header, a date header and two history items.
        checkAdapterContents(mAdapter, true, false, null, null, item1, item2);

        mAdapter.markItemForRemoval(item1);

        // Check that one item was removed.
        checkAdapterContents(mAdapter, true, false, null, null, item2);
        Assert.assertEquals(1, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        Assert.assertEquals(0, mHistoryProvider.removeItemsCallback.getCallCount());

        mAdapter.markItemForRemoval(item2);

        // There should no longer be any items in the adapter.
        checkAdapterContents(mAdapter, false, false);
        Assert.assertEquals(2, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        Assert.assertEquals(0, mHistoryProvider.removeItemsCallback.getCallCount());

        mAdapter.removeItems();
        Assert.assertEquals(1, mHistoryProvider.removeItemsCallback.getCallCount());
    }

    @Test
    public void testRemove_TwoItemsTwoDates() {
        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mHistoryProvider.addItem(item1);

        long timestamp2 = today.getTime() - TimeUnit.DAYS.toMillis(3);
        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, timestamp2);
        mHistoryProvider.addItem(item2);

        mAdapter.startLoadingItems();

        // There should be five items - the list header, a date header, a history item, another
        // date header and another history item.
        checkAdapterContents(mAdapter, true, false, null, null, item1, null, item2);

        mAdapter.markItemForRemoval(item1);

        // Check that the first item and date header were removed.
        checkAdapterContents(mAdapter, true, false, null, null, item2);
        Assert.assertEquals(1, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        Assert.assertEquals(0, mHistoryProvider.removeItemsCallback.getCallCount());

        mAdapter.markItemForRemoval(item2);

        // There should no longer be any items in the adapter.
        checkAdapterContents(mAdapter, false, false);
        Assert.assertEquals(2, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        Assert.assertEquals(0, mHistoryProvider.removeItemsCallback.getCallCount());

        mAdapter.removeItems();
        Assert.assertEquals(1, mHistoryProvider.removeItemsCallback.getCallCount());
    }

    @Test
    public void testSearch() {
        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mHistoryProvider.addItem(item1);

        long timestamp2 = today.getTime() - TimeUnit.DAYS.toMillis(3);
        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, timestamp2);
        mHistoryProvider.addItem(item2);

        mAdapter.startLoadingItems();
        checkAdapterContents(mAdapter, true, false, null, null, item1, null, item2);

        mAdapter.search("google");

        // The header should be hidden during the search.
        checkAdapterContents(mAdapter, false, false, null, item1);

        mAdapter.onEndSearch();

        // The header should be shown again after the search.
        checkAdapterContents(mAdapter, true, false, null, null, item1, null, item2);
    }

    @Test
    public void testLoadMoreItems() {
        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mHistoryProvider.addItem(item1);

        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, timestamp);
        mHistoryProvider.addItem(item2);

        HistoryItem item3 = StubbedHistoryProvider.createHistoryItem(2, timestamp);
        mHistoryProvider.addItem(item3);

        HistoryItem item4 = StubbedHistoryProvider.createHistoryItem(3, timestamp);
        mHistoryProvider.addItem(item4);

        long timestamp2 = today.getTime() - TimeUnit.DAYS.toMillis(2);
        HistoryItem item5 = StubbedHistoryProvider.createHistoryItem(4, timestamp2);
        mHistoryProvider.addItem(item5);

        HistoryItem item6 = StubbedHistoryProvider.createHistoryItem(0, timestamp2);
        mHistoryProvider.addItem(item6);

        long timestamp3 = today.getTime() - TimeUnit.DAYS.toMillis(4);
        HistoryItem item7 = StubbedHistoryProvider.createHistoryItem(1, timestamp3);
        mHistoryProvider.addItem(item7);

        mAdapter.startLoadingItems();

        // Only the first five of the seven items should be loaded.
        checkAdapterContents(
                mAdapter, true, false, null, null, item1, item2, item3, item4, null, item5);
        Assert.assertTrue(mAdapter.canLoadMoreItems());

        mAdapter.loadMoreItems();

        // All items should now be loaded.
        checkAdapterContents(
                mAdapter, true, false, null, null, item1, item2, item3, item4, null, item5, item6,
                null, item7);
        Assert.assertFalse(mAdapter.canLoadMoreItems());
    }

    @Test
    public void testOnHistoryDeleted() {
        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mHistoryProvider.addItem(item1);

        mAdapter.startLoadingItems();

        checkAdapterContents(mAdapter, true, false, null, null, item1);

        mHistoryProvider.removeItem(item1);

        mAdapter.onHistoryDeleted();

        checkAdapterContents(mAdapter, false, false);
    }

    @Test
    public void testBlockedSite() {
        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mHistoryProvider.addItem(item1);

        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(5, timestamp);
        mHistoryProvider.addItem(item2);

        mAdapter.startLoadingItems();

        checkAdapterContents(mAdapter, true, false, null, null, item1, item2);
        Assert.assertEquals(
                ContextUtils.getApplicationContext()
                        .getString(R.string.android_history_blocked_site),
                item2.getTitle());
        Assert.assertTrue(item2.wasBlockedVisit());
    }
}
