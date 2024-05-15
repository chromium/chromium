// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.history.HistoryTestUtils.checkAdapterContents;

import android.view.View;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.history.AppFilterCoordinator.AppInfo;
import org.chromium.components.browser_ui.widget.MoreProgressButton;
import org.chromium.components.browser_ui.widget.chips.ChipView;

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
    @Mock private ChipView mAppFilterChip;
    @Mock private TextView mTextView;
    @Mock private View mAppFilterContainer;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mHistoryProvider = new StubbedHistoryProvider();
        mAdapter = new HistoryAdapter(mContentManager, mHistoryProvider);
        mAdapter.generateHeaderItemsForTest();
        mAdapter.generateFooterItemsForTest(mMockButton);
        doReturn(mTextView).when(mAppFilterChip).getPrimaryTextView();
    }

    private void initializeAdapter() {
        mAdapter.startLoadingItems();
    }

    private boolean showSourceApp() {
        return mAdapter.showSourceAppForTest();
    }

    private String getAppId() {
        return mAdapter.getAppIdForTest();
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
    public void testHideSourceAppInAppSpecficHistory() {
        // For the entire lifecycle, source app should remain hidden for app-specific history.
        final String appId = "org.nicecompany.niceapp";
        Assert.assertFalse("Source app should be hidden", showSourceApp());

        mAdapter.setAppId(appId);

        mAdapter.onSearchStart();
        Assert.assertFalse("Source app should remain hidden on entering search", showSourceApp());
        Assert.assertEquals("App id should remain unchanged on entering search", appId, getAppId());

        mAdapter.onEndSearch();
        Assert.assertFalse("Source app should remain hidden on exiting search", showSourceApp());
        Assert.assertEquals("App id should remain unchanged on exiting search", appId, getAppId());
    }

    @Test
    public void testShowSourceAppInBrAppHistory() {
        // Reinstantiate HistoryAdapter with |showAppFilter| flag on. Now we're in BrApp.
        doReturn(true).when(mContentManager).showAppFilter();
        mAdapter = new HistoryAdapter(mContentManager, mHistoryProvider);

        mAdapter.generateHeaderItemsForTest();
        mAdapter.generateFooterItemsForTest(mMockButton);
        mAdapter.setAppFilterButtonForTest(mAppFilterChip);
        Assert.assertTrue("Source app should be on", showSourceApp());
        Assert.assertEquals("App id should be null", null, getAppId());

        // |onSearchStart| is called when search mode is entered. This should
        // not affect the show-source-app flag.
        mAdapter.onSearchStart();
        Assert.assertTrue("Source app should remain on when entering search", showSourceApp());

        mAdapter.updateHistory(new AppInfo("org.great.app", null, "Great App"));
        Assert.assertFalse("No source app when app filter is on", showSourceApp());
        Assert.assertEquals("App id should switch to greatapp", "org.great.app", getAppId());

        mAdapter.updateHistory(null);
        Assert.assertTrue("Source app when app filter is reset", showSourceApp());
        Assert.assertEquals("App id should switch to null", null, getAppId());

        mAdapter.updateHistory(new AppInfo("org.awesome.app", null, "Awesome App"));
        Assert.assertFalse("No source app when app filter is on again", showSourceApp());
        Assert.assertEquals("App id should switch to awesomeapp", "org.awesome.app", getAppId());

        mAdapter.onEndSearch();
        Assert.assertTrue("Should show source app when search is exited", showSourceApp());
        Assert.assertEquals("App id should reverted to null", null, getAppId());
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
