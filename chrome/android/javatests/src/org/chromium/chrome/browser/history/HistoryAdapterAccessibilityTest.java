// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.chromium.chrome.browser.history.HistoryTestUtils.checkAdapterContents;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.ui.widget.MoreProgressButton;
import org.chromium.chrome.browser.ui.widget.MoreProgressButton.State;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Date;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the {@link HistoryAdapter}. This test will more focusing on cases when accessibility
 * turned on (HistoryManager::isScrollToLoadDisabled() == true).
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/1023426")
public class HistoryAdapterAccessibilityTest {
    public static final int PAGING = 2;

    private StubbedHistoryProvider mHistoryProvider;
    private HistoryAdapter mAdapter;

    @Mock
    private MoreProgressButton mMockButton;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mHistoryProvider = new StubbedHistoryProvider();
        mHistoryProvider.setPaging(PAGING);

        mAdapter = new HistoryAdapter(new SelectionDelegate<HistoryItem>(), null, mHistoryProvider);
        mAdapter.generateHeaderItemsForTest();
        mAdapter.generateFooterItemsForTest(mMockButton);
        mAdapter.setScrollToLoadDisabledForTest(true);
    }

    private void initializeAdapter() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mAdapter.initialize());
    }

    @Test
    @SmallTest
    public void testInitializeEmpty() {
        initializeAdapter();
        checkAdapterContents(mAdapter, false, false);
    }

    @Test
    @SmallTest
    public void testInitializeSingleItem() {
        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mHistoryProvider.addItem(item1);

        initializeAdapter();

        // There should be three items - the header, a date and the history item;
        // The number of items is less than paging, so the view should not contain footer items.
        checkAdapterContents(mAdapter, true, false, null, null, item1);
    }

    @Test
    @SmallTest
    public void testInitializeThreeItems() {
        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mHistoryProvider.addItem(item1);

        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, timestamp);
        mHistoryProvider.addItem(item2);

        HistoryItem item3 = StubbedHistoryProvider.createHistoryItem(2, timestamp);
        mHistoryProvider.addItem(item3);

        initializeAdapter();

        // There should be five items - the header, a date, two history item, and a footer;
        checkAdapterContents(mAdapter, true, true, null, null, item1, item2, null);

        // Footer should be set as button after initial load.
        Mockito.verify(mMockButton, Mockito.times(1)).setState(State.BUTTON);
    }

    @Test
    @SmallTest
    public void testRemoveItemsWithFooter() {
        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mHistoryProvider.addItem(item1);

        long timestamp2 = today.getTime() - TimeUnit.DAYS.toMillis(3);
        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, timestamp2);
        mHistoryProvider.addItem(item2);

        long timestamp3 = today.getTime() - TimeUnit.DAYS.toMillis(5);
        HistoryItem item3 = StubbedHistoryProvider.createHistoryItem(2, timestamp2);
        mHistoryProvider.addItem(item3);

        initializeAdapter();

        // There should be six items - the list header, a date header, a history item, another
        // date header, another history item, and the footer.
        checkAdapterContents(mAdapter, true, true, null, null, item1, null, item2, null);

        mAdapter.markItemForRemoval(item1);

        // Check that the first item and date header were removed. Footer will persists
        checkAdapterContents(mAdapter, true, true, null, null, item2, null);
        Assert.assertEquals(1, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        Assert.assertEquals(0, mHistoryProvider.removeItemsCallback.getCallCount());

        mAdapter.markItemForRemoval(item2);

        // There should no longer be any items in the adapter;
        // However, the header and footer persists
        checkAdapterContents(mAdapter, true, true, null, null);
        Assert.assertEquals(2, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        Assert.assertEquals(0, mHistoryProvider.removeItemsCallback.getCallCount());

        // After removing all items, the view will refresh and the 3rd item will be displayed
        mAdapter.removeItems();
        checkAdapterContents(mAdapter, true, false, null, null, item3);
        Assert.assertEquals(1, mHistoryProvider.removeItemsCallback.getCallCount());

        // Mark the 3rd item removed. The view should be empty.
        mAdapter.markItemForRemoval(item3);
        checkAdapterContents(mAdapter, false, false);
        Assert.assertEquals(3, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        Assert.assertEquals(1, mHistoryProvider.removeItemsCallback.getCallCount());

        mAdapter.removeItems();
        checkAdapterContents(mAdapter, false, false);
        Assert.assertEquals(2, mHistoryProvider.removeItemsCallback.getCallCount());
    }

    @Test
    @SmallTest
    public void testSearchWithFooter() {
        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mHistoryProvider.addItem(item1);

        long timestamp2 = today.getTime() - TimeUnit.DAYS.toMillis(3);
        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, timestamp2);
        mHistoryProvider.addItem(item2);

        long timestamp3 = today.getTime() - TimeUnit.DAYS.toMillis(5);
        HistoryItem item3 = StubbedHistoryProvider.createHistoryItem(0, timestamp3);
        mHistoryProvider.addItem(item3);

        long timestamp4 = today.getTime() - TimeUnit.DAYS.toMillis(7);
        HistoryItem item4 = StubbedHistoryProvider.createHistoryItem(0, timestamp4);
        mHistoryProvider.addItem(item4);

        initializeAdapter();
        checkAdapterContents(mAdapter, true, true, null, null, item1, null, item2, null);

        mAdapter.search("google");

        // The header should be hidden during the search;
        // The 1st, 3rd, 4th item match the search, we'll have two of them displayed after search
        // due to the limit of paging
        // Since there are more items to be displayed, footer will be on screen  after search
        checkAdapterContents(mAdapter, false, true, null, item1, null, item3, null);

        mAdapter.onEndSearch();

        // The header should be shown again after the search.
        checkAdapterContents(mAdapter, true, true, null, null, item1, null, item2, null);
    }

    @Test
    @SmallTest
    public void testSearchWithoutFooter() {
        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mHistoryProvider.addItem(item1);

        long timestamp2 = today.getTime() - TimeUnit.DAYS.toMillis(3);
        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, timestamp2);
        mHistoryProvider.addItem(item2);

        long timestamp3 = today.getTime() - TimeUnit.DAYS.toMillis(5);
        HistoryItem item3 = StubbedHistoryProvider.createHistoryItem(0, timestamp3);
        mHistoryProvider.addItem(item3);

        long timestamp4 = today.getTime() - TimeUnit.DAYS.toMillis(7);
        HistoryItem item4 = StubbedHistoryProvider.createHistoryItem(1, timestamp4);
        mHistoryProvider.addItem(item4);

        initializeAdapter();
        checkAdapterContents(mAdapter, true, true, null, null, item1, null, item2, null);

        mAdapter.search("google");

        // The header should be hidden during the search;
        // The 1st, 3rd items match the search, we'll have both of them displayed after search
        // There are no more items to be displayed, footer should not be on screen after search
        checkAdapterContents(mAdapter, false, false, null, item1, null, item3);

        mAdapter.onEndSearch();

        // The header should be shown again after the search.
        checkAdapterContents(mAdapter, true, true, null, null, item1, null, item2, null);
    }

    @Test
    @SmallTest
    public void testLoadMoreItems() {
        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mHistoryProvider.addItem(item1);

        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, timestamp);
        mHistoryProvider.addItem(item2);

        long timestamp2 = today.getTime() - TimeUnit.DAYS.toMillis(2);
        HistoryItem item3 = StubbedHistoryProvider.createHistoryItem(3, timestamp2);
        mHistoryProvider.addItem(item3);

        HistoryItem item4 = StubbedHistoryProvider.createHistoryItem(4, timestamp2);
        mHistoryProvider.addItem(item4);

        initializeAdapter();

        // Only the first 2 of five items should be loaded.
        checkAdapterContents(mAdapter, true, true, null, null, item1, item2, null);
        Assert.assertTrue(mAdapter.canLoadMoreItems());

        mAdapter.loadMoreItems();

        // All items should now be loaded.
        // Since hasMoreItemsToLoad = false, the footer should be set to hidden
        checkAdapterContents(mAdapter, true, false, null, null, item1, item2, null, item3, item4);
        Assert.assertFalse(mAdapter.canLoadMoreItems());
    }

    @Test
    @SmallTest
    public void testLoadMoreItemsInSearch() {
        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mHistoryProvider.addItem(item1);

        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, timestamp);
        mHistoryProvider.addItem(item2);

        long timestamp3 = today.getTime() - TimeUnit.DAYS.toMillis(3);
        HistoryItem item3 = StubbedHistoryProvider.createHistoryItem(0, timestamp3);
        mHistoryProvider.addItem(item3);

        long timestamp4 = today.getTime() - TimeUnit.DAYS.toMillis(5);
        HistoryItem item4 = StubbedHistoryProvider.createHistoryItem(0, timestamp4);
        mHistoryProvider.addItem(item4);

        initializeAdapter();
        checkAdapterContents(mAdapter, true, true, null, null, item1, item2, null);

        mAdapter.search("google");

        // The header should be hidden during the search;
        // The 1st, 3rd, 4th items match the search result, we'll only have two of them displayed
        // Since there are more items to be displayed, footer will be on screen
        checkAdapterContents(mAdapter, false, true, null, item1, null, item3, null);

        mAdapter.loadMoreItems();

        // All items should now be loaded.
        checkAdapterContents(mAdapter, false, false, null, item1, null, item3, null, item4);
        Assert.assertFalse(mAdapter.canLoadMoreItems());
    }
}
