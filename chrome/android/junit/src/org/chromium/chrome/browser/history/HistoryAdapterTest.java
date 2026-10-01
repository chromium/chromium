// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.history.HistoryTestUtils.checkAdapterContents;

import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnPreDrawListener;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.history.FilterSheetCoordinator.FilterItem;
import org.chromium.chrome.browser.ui.signin.signin_promo.SigninPromoCoordinator;
import org.chromium.components.browser_ui.widget.MoreProgressButton;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for the {@link HistoryAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // TODO(567604165): Remove mocking of Views / Activities
public class HistoryAdapterTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private StubbedHistoryProvider mHistoryProvider;
    private HistoryAdapter mAdapter;

    @Mock private MoreProgressButton mMockButton;
    @Mock private HistoryContentManager mContentManager;
    @Mock private SigninPromoCoordinator mHistorySyncPromoCoordinator;
    @Mock private RecyclerView mRecyclerView;
    @Mock private ViewTreeObserver mViewTreeObserver;
    @Captor private ArgumentCaptor<OnPreDrawListener> mOnPreDrawListenerCaptor;

    @Before
    public void setUp() {
        lenient().doReturn(new HistoryUmaRecorder()).when(mContentManager).getUmaRecorder();
        lenient().doReturn(mViewTreeObserver).when(mRecyclerView).getViewTreeObserver();
        mHistoryProvider = new StubbedHistoryProvider();
        mAdapter =
                new HistoryAdapter(
                        mContentManager,
                        mHistoryProvider,
                        mHistorySyncPromoCoordinator,
                        /* shouldClusterByDomain= */ false,
                        /* snackbarManager= */ null,
                        /* profile= */ null);
        mAdapter.generateHeaderItemsForTest();
        mAdapter.generateFooterItemsForTest(mMockButton);
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
        mAdapter =
                new HistoryAdapter(
                        mContentManager,
                        mHistoryProvider,
                        mHistorySyncPromoCoordinator,
                        /* shouldClusterByDomain= */ false,
                        /* snackbarManager= */ null,
                        /* profile= */ null);

        mAdapter.generateHeaderItemsForTest();
        mAdapter.generateFooterItemsForTest(mMockButton);
        Assert.assertTrue("Source app should be on", showSourceApp());
        Assert.assertEquals("App id should be null", null, getAppId());

        // |onSearchStart| is called when search mode is entered. This should
        // not affect the show-source-app flag.
        mAdapter.onSearchStart();
        Assert.assertTrue("Source app should remain on when entering search", showSourceApp());

        mAdapter.updateAppFilter(new FilterItem("org.great.app", null, "Great App"));
        Assert.assertFalse("No source app when app filter is on", showSourceApp());
        Assert.assertEquals("App id should switch to greatapp", "org.great.app", getAppId());

        mAdapter.updateAppFilter(null);
        Assert.assertTrue("Source app when app filter is reset", showSourceApp());
        Assert.assertEquals("App id should switch to null", null, getAppId());

        mAdapter.updateAppFilter(new FilterItem("org.awesome.app", null, "Awesome App"));
        Assert.assertFalse("No source app when app filter is on again", showSourceApp());
        Assert.assertEquals("App id should switch to awesomeapp", "org.awesome.app", getAppId());

        mAdapter.onEndSearch();
        Assert.assertTrue("Should show source app when search is exited", showSourceApp());
        Assert.assertEquals("App id should reverted to null", null, getAppId());
    }

    @Test
    public void testSearch_HostAndClientFilter() {
        doReturn(true).when(mContentManager).showAppFilter();
        doReturn(true).when(mContentManager).showHostFilter();
        doReturn(true).when(mContentManager).showClientFilter();
        mAdapter =
                new HistoryAdapter(
                        mContentManager,
                        mHistoryProvider,
                        mHistorySyncPromoCoordinator,
                        /* shouldClusterByDomain= */ false,
                        /* snackbarManager= */ null,
                        /* profile= */ null);
        mAdapter.generateHeaderItemsForTest();
        mAdapter.generateFooterItemsForTest(mMockButton);

        mAdapter.onSearchStart();
        Assert.assertNull(mAdapter.getHostNameForTest());
        Assert.assertTrue(mAdapter.getClientIdsForTest().isEmpty());

        mAdapter.updateHostFilter(new FilterItem("www.google.com", null, "www.google.com"));
        Assert.assertEquals("www.google.com", mAdapter.getHostNameForTest());
        Assert.assertEquals(
                new QueryOptions(null, "www.google.com", Collections.emptyList()),
                mHistoryProvider.getLastQueryOptions());

        mAdapter.updateClientFilter(
                new FilterItem(List.of("client_123", "client_456"), null, "Pixel 8"));
        Assert.assertEquals(List.of("client_123", "client_456"), mAdapter.getClientIdsForTest());
        Assert.assertEquals(
                new QueryOptions(null, "www.google.com", List.of("client_123", "client_456")),
                mHistoryProvider.getLastQueryOptions());

        mAdapter.onEndSearch();
        Assert.assertNull(mAdapter.getHostNameForTest());
        Assert.assertTrue(mAdapter.getClientIdsForTest().isEmpty());
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
    public void testRemove_WithPersistentHeader() {
        // Add one history item.
        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        mHistoryProvider.addItem(item);
        // Show the history sync promo header.
        doReturn(true).when(mHistorySyncPromoCoordinator).canShowPromo();
        mAdapter.updateHistorySyncPromoVisibility();

        mAdapter.startLoadingItems();

        // There should be three items - the standard and persistent headers, a date header and
        // a history item.
        checkAdapterContents(
                mAdapter,
                /* hasStandardHeader= */ true,
                /* hasPersistentHeader= */ true,
                /* hasFooter= */ false,
                null,
                null,
                null,
                item);

        mAdapter.markItemForRemoval(item);

        // The standard and persistent headers should be the only visible items.
        checkAdapterContents(
                mAdapter,
                /* hasStandardHeader= */ true,
                /* hasPersistentHeader= */ true,
                /* hasFooter= */ false,
                null,
                null);
        Assert.assertEquals(1, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        Assert.assertEquals(0, mHistoryProvider.removeItemsCallback.getCallCount());

        mAdapter.removeItems();
        Assert.assertEquals(1, mHistoryProvider.removeItemsCallback.getCallCount());
    }

    @Test
    public void testRemovePersistentHeader() {
        // Show the history sync promo header.
        doReturn(true).when(mHistorySyncPromoCoordinator).canShowPromo();
        mAdapter.updateHistorySyncPromoVisibility();

        mAdapter.startLoadingItems();

        // The standard and persistent headers should be the only visible items.
        checkAdapterContents(
                mAdapter,
                /* hasStandardHeader= */ true,
                /* hasPersistentHeader= */ true,
                /* hasFooter= */ false,
                null,
                null);

        // Hide the history sync promo header.
        doReturn(false).when(mHistorySyncPromoCoordinator).canShowPromo();
        mAdapter.updateHistorySyncPromoVisibility();

        // No item should be shown.
        checkAdapterContents(
                mAdapter,
                /* hasStandardHeader= */ false,
                /* hasPersistentHeader= */ false,
                /* hasFooter= */ false);
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

    @Test
    public void testClusteringByDomain() {
        // Re-instantiate adapter with clustering enabled
        mAdapter =
                new HistoryAdapter(
                        mContentManager,
                        mHistoryProvider,
                        mHistorySyncPromoCoordinator,
                        /* shouldClusterByDomain= */ true,
                        /* snackbarManager= */ null,
                        /* profile= */ null);
        mAdapter.generateHeaderItemsForTest();

        Date today = new Date();
        long timestamp = today.getTime();
        // Two items from google.com
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(0, timestamp - 100);
        // One item from example.com
        HistoryItem item3 = StubbedHistoryProvider.createHistoryItem(1, timestamp - 200);

        mHistoryProvider.addItem(item1);
        mHistoryProvider.addItem(item2);
        mHistoryProvider.addItem(item3);

        mAdapter.startLoadingItems();

        // The items should be clustered.
        // Expected displayed items: Header, Date, item1, item3
        checkAdapterContents(mAdapter, true, false, null, null, item1, item3);
        HistoryItem clusterHead1 = (HistoryItem) mAdapter.getItemAt(2).second;
        Assert.assertTrue(clusterHead1.isClusterHead());
        Assert.assertNotNull(clusterHead1.getSubItems());
        Assert.assertEquals(2, clusterHead1.getSubItems().size());
        Assert.assertEquals(item1.getUrl(), clusterHead1.getSubItems().get(0).getUrl());
        Assert.assertEquals(item2.getUrl(), clusterHead1.getSubItems().get(1).getUrl());

        // Toggle expansion
        mAdapter.toggleCluster(clusterHead1);
        // Expected display: Header, Date, head(item1), item1, item2, item3
        checkAdapterContents(mAdapter, true, false, null, null, item1, item1, item2, item3);
        clusterHead1 = (HistoryItem) mAdapter.getItemAt(2).second;
        Assert.assertTrue(clusterHead1.isExpanded());

        // Toggle back
        mAdapter.toggleCluster(clusterHead1);
        checkAdapterContents(mAdapter, true, false, null, null, item1, item3);
        clusterHead1 = (HistoryItem) mAdapter.getItemAt(2).second;
        Assert.assertFalse(clusterHead1.isExpanded());
    }

    @Test
    public void testRemoveClusterHead() {
        mAdapter =
                new HistoryAdapter(
                        mContentManager,
                        mHistoryProvider,
                        mHistorySyncPromoCoordinator,
                        /* shouldClusterByDomain= */ true,
                        /* snackbarManager= */ null,
                        /* profile= */ null);
        mAdapter.generateHeaderItemsForTest();

        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(0, timestamp - 100);
        mHistoryProvider.addItem(item1);
        mHistoryProvider.addItem(item2);

        mAdapter.startLoadingItems();
        // Header, Date, item1 (cluster head)
        checkAdapterContents(mAdapter, true, false, null, null, item1);

        HistoryItem clusterHead = (HistoryItem) mAdapter.getItemAt(2).second;
        mAdapter.markItemForRemoval(clusterHead);

        // Both item1 and item2 should be removed.
        checkAdapterContents(mAdapter, false, false);
        Assert.assertEquals(2, mHistoryProvider.markItemForRemovalCallback.getCallCount());
    }

    @Test
    public void testRemoveSubItem() {
        mAdapter =
                new HistoryAdapter(
                        mContentManager,
                        mHistoryProvider,
                        mHistorySyncPromoCoordinator,
                        /* shouldClusterByDomain= */ true,
                        /* snackbarManager= */ null,
                        /* profile= */ null);
        mAdapter.generateHeaderItemsForTest();

        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(0, timestamp - 100);
        HistoryItem item3 = StubbedHistoryProvider.createHistoryItem(0, timestamp - 200);
        mHistoryProvider.addItem(item1);
        mHistoryProvider.addItem(item2);
        mHistoryProvider.addItem(item3);

        mAdapter.startLoadingItems();
        // item1 is cluster head, item2 and item3 are sub-items.
        // display: Header, Date, item1
        checkAdapterContents(mAdapter, true, false, null, null, item1);
        HistoryItem clusterHead1 = (HistoryItem) mAdapter.getItemAt(2).second;
        Assert.assertEquals(3, clusterHead1.getSubItems().size());

        // Remove item2 (a sub-item)
        HistoryItem actualItem2 = clusterHead1.getSubItems().get(1);
        mAdapter.markItemForRemoval(actualItem2);
        // item1 should still be cluster head but with only item3 as sub-item.
        checkAdapterContents(mAdapter, true, false, null, null, item1);
        clusterHead1 = (HistoryItem) mAdapter.getItemAt(2).second;
        Assert.assertEquals(2, clusterHead1.getSubItems().size());
        Assert.assertEquals(item1.getUrl(), clusterHead1.getSubItems().get(0).getUrl());
        Assert.assertEquals(item3.getUrl(), clusterHead1.getSubItems().get(1).getUrl());

        // Remove item3
        HistoryItem actualItem3 = clusterHead1.getSubItems().get(1);
        mAdapter.markItemForRemoval(actualItem3);
        // only item1 left, it shouldn't be a cluster head anymore.
        checkAdapterContents(mAdapter, true, false, null, null, item1);
        clusterHead1 = (HistoryItem) mAdapter.getItemAt(2).second;
        Assert.assertFalse(clusterHead1.isClusterHead());
    }

    @Test
    public void testStableIdInvariantOnClusterDeletion() {
        mAdapter =
                new HistoryAdapter(
                        mContentManager,
                        mHistoryProvider,
                        mHistorySyncPromoCoordinator,
                        /* shouldClusterByDomain= */ true,
                        /* snackbarManager= */ null,
                        /* profile= */ null);
        mAdapter.generateHeaderItemsForTest();

        Date today = new Date();
        long timestamp = today.getTime();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(0, timestamp - 100);
        HistoryItem item3 = StubbedHistoryProvider.createHistoryItem(0, timestamp - 200);
        mHistoryProvider.addItem(item1);
        mHistoryProvider.addItem(item2);
        mHistoryProvider.addItem(item3);

        mAdapter.startLoadingItems();
        // The cluster head is the first item returned.
        HistoryItem clusterHeadBefore = (HistoryItem) mAdapter.getItemAt(2).second;
        long stableIdBefore = clusterHeadBefore.getStableId();

        // Remove item1, which is the first sub-item and the template for the virtual head.
        HistoryItem actualItem1 = clusterHeadBefore.getSubItems().get(0);
        mAdapter.markItemForRemoval(actualItem1);

        // Get the cluster head again.
        HistoryItem clusterHeadAfter = (HistoryItem) mAdapter.getItemAt(2).second;
        long stableIdAfter = clusterHeadAfter.getStableId();

        // The stable ID should remain the same even after the first sub-item is deleted.
        Assert.assertEquals(
                "Stable ID should be invariant when first sub-item is deleted",
                stableIdBefore,
                stableIdAfter);
    }

    @Test
    public void testDistinctClustersSameDomain() {
        mAdapter =
                new HistoryAdapter(
                        mContentManager,
                        mHistoryProvider,
                        mHistorySyncPromoCoordinator,
                        /* shouldClusterByDomain= */ true,
                        /* snackbarManager= */ null,
                        /* profile= */ null);
        mAdapter.generateHeaderItemsForTest();

        Date today = new Date();
        long timestamp = today.getTime();

        // Cluster 1 (Google)
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, timestamp);
        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(0, timestamp - 100);

        // Interrupting item (Example)
        HistoryItem item3 = StubbedHistoryProvider.createHistoryItem(1, timestamp - 200);

        // Cluster 2 (Google, same day)
        HistoryItem item4 = StubbedHistoryProvider.createHistoryItem(0, timestamp - 300);
        HistoryItem item5 = StubbedHistoryProvider.createHistoryItem(0, timestamp - 400);

        mHistoryProvider.addItem(item1);
        mHistoryProvider.addItem(item2);
        mHistoryProvider.addItem(item3);
        mHistoryProvider.addItem(item4);
        mHistoryProvider.addItem(item5);

        mAdapter.startLoadingItems();

        // The items should form two separate clusters for Google.
        // Expected displayed items: Header, Date, item1 (head), item3, item4 (head)
        checkAdapterContents(mAdapter, true, false, null, null, item1, item3, item4);

        HistoryItem clusterHead1 = (HistoryItem) mAdapter.getItemAt(2).second;
        HistoryItem clusterHead4 = (HistoryItem) mAdapter.getItemAt(4).second;

        Assert.assertTrue(clusterHead1.isClusterHead());
        Assert.assertTrue(clusterHead4.isClusterHead());

        Assert.assertNotEquals(
                "Distinct clusters should have different stable IDs",
                clusterHead1.getStableId(),
                clusterHead4.getStableId());

        // Toggle expansion of the first cluster
        mAdapter.toggleCluster(clusterHead1);

        // Expected display: Header, Date, item1 (head), item1, item2, item3, item4 (head)
        checkAdapterContents(mAdapter, true, false, null, null, item1, item1, item2, item3, item4);

        clusterHead1 = (HistoryItem) mAdapter.getItemAt(2).second;
        clusterHead4 = (HistoryItem) mAdapter.getItemAt(6).second;

        Assert.assertTrue(clusterHead1.isExpanded());
        Assert.assertFalse(
                "Expanding one cluster should not expand the other independent cluster",
                clusterHead4.isExpanded());
    }

    @Test
    public void testSearch_SetsIsLoadingItems() {
        HistoryProvider mockProvider = Mockito.mock(HistoryProvider.class);
        mAdapter =
                new HistoryAdapter(
                        mContentManager,
                        mockProvider,
                        mHistorySyncPromoCoordinator,
                        /* shouldClusterByDomain= */ false,
                        /* snackbarManager= */ null,
                        /* profile= */ null);

        Assert.assertFalse(mAdapter.canLoadMoreItems());

        mAdapter.search("query");

        Mockito.verify(mockProvider).queryHistory("query", new QueryOptions());
        // While the query is ongoing, no more items can be loaded.
        Assert.assertFalse(mAdapter.canLoadMoreItems());

        // Once the query completes (and indicates that more results are
        // available), more items can be loaded.
        mAdapter.onQueryHistoryComplete(new ArrayList<>(), true);
        Assert.assertTrue(mAdapter.canLoadMoreItems());
    }

    @Test
    public void testQueryDurationHistogram() {
        HistogramWatcher initialWatcher =
                HistogramWatcher.newSingleRecordWatcher("Android.HistoryPage.QueryDuration");
        mAdapter.startLoadingItems();
        initialWatcher.assertExpected();

        // Continuation query should also record QueryDuration.
        mHistoryProvider.setPaging(2);
        for (int i = 0; i < 5; ++i) {
            mHistoryProvider.addItem(
                    StubbedHistoryProvider.createHistoryItem(i, new Date().getTime()));
        }
        mAdapter.startLoadingItems();

        HistogramWatcher continuationWatcher =
                HistogramWatcher.newSingleRecordWatcher("Android.HistoryPage.QueryDuration");
        mAdapter.loadMoreItems();
        continuationWatcher.assertExpected();

        // Search query should also record QueryDuration.
        HistogramWatcher searchWatcher =
                HistogramWatcher.newSingleRecordWatcher("Android.HistoryPage.QueryDuration");
        mAdapter.search("query");
        searchWatcher.assertExpected();
    }

    @Test
    public void testTimeToFirstVisibleContentHistogram() {
        mAdapter.onAttachedToRecyclerView(mRecyclerView);

        Date today = new Date();
        HistoryItem item = StubbedHistoryProvider.createHistoryItem(0, today.getTime());
        mHistoryProvider.addItem(item);

        HistogramWatcher durationWatcher =
                HistogramWatcher.newSingleRecordWatcher("Android.HistoryPage.QueryDuration");
        HistogramWatcher fcpWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.HistoryPage.TimeToFirstVisibleContent");

        mAdapter.startLoadingItems();

        durationWatcher.assertExpected();

        verify(mViewTreeObserver).addOnPreDrawListener(mOnPreDrawListenerCaptor.capture());
        mOnPreDrawListenerCaptor.getValue().onPreDraw();

        fcpWatcher.assertExpected();
        verify(mViewTreeObserver).removeOnPreDrawListener(mOnPreDrawListenerCaptor.getValue());
    }

    @Test
    public void testTimeToFirstVisibleContent_EmptyListDoesNotRecordAndResetsTimestamp() {
        mAdapter.onAttachedToRecyclerView(mRecyclerView);

        // Initial load with empty history.
        HistogramWatcher emptyWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Android.HistoryPage.QueryDuration")
                        .expectNoRecords("Android.HistoryPage.TimeToFirstVisibleContent")
                        .build();

        mAdapter.startLoadingItems();

        emptyWatcher.assertExpected();
        verify(mViewTreeObserver, never()).addOnPreDrawListener(any(OnPreDrawListener.class));

        // Subsequent query after adding items must NOT fire initial TimeToFirstVisibleContent.
        Date today = new Date();
        mHistoryProvider.addItem(StubbedHistoryProvider.createHistoryItem(0, today.getTime()));

        HistogramWatcher searchWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Android.HistoryPage.QueryDuration")
                        .expectNoRecords("Android.HistoryPage.TimeToFirstVisibleContent")
                        .build();

        mAdapter.search("test");

        searchWatcher.assertExpected();
        verify(mViewTreeObserver, never()).addOnPreDrawListener(any(OnPreDrawListener.class));
    }

    @Test
    public void testTimeToFirstVisibleContent_DestroyedDoesNotRecord() {
        mAdapter.onAttachedToRecyclerView(mRecyclerView);

        Date today = new Date();
        HistoryItem item = StubbedHistoryProvider.createHistoryItem(0, today.getTime());
        mHistoryProvider.addItem(item);

        mAdapter.startLoadingItems();
        verify(mViewTreeObserver).addOnPreDrawListener(mOnPreDrawListenerCaptor.capture());

        // Destroy the adapter before the pre-draw callback fires.
        mAdapter.onDestroyed();

        HistogramWatcher fcpWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.HistoryPage.TimeToFirstVisibleContent")
                        .build();

        Assert.assertTrue(mOnPreDrawListenerCaptor.getValue().onPreDraw());
        fcpWatcher.assertExpected();
        verify(mViewTreeObserver).removeOnPreDrawListener(mOnPreDrawListenerCaptor.getValue());
    }

    @Test
    public void testTimeToFirstVisibleContent_OnlyRecordedOnInitialLoad() {
        mAdapter.onAttachedToRecyclerView(mRecyclerView);

        Date today = new Date();
        HistoryItem item = StubbedHistoryProvider.createHistoryItem(0, today.getTime());
        mHistoryProvider.addItem(item);

        mAdapter.startLoadingItems();
        verify(mViewTreeObserver).addOnPreDrawListener(mOnPreDrawListenerCaptor.capture());
        mOnPreDrawListenerCaptor.getValue().onPreDraw();

        // Subsequent reloads via startLoadingItems(), onEndSearch(), and onHistoryDeleted()
        // should record QueryDuration but not TimeToFirstVisibleContent.
        HistogramWatcher reloadWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Android.HistoryPage.QueryDuration")
                        .expectNoRecords("Android.HistoryPage.TimeToFirstVisibleContent")
                        .build();
        mAdapter.startLoadingItems();
        reloadWatcher.assertExpected();

        HistogramWatcher endSearchWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Android.HistoryPage.QueryDuration")
                        .expectNoRecords("Android.HistoryPage.TimeToFirstVisibleContent")
                        .build();
        mAdapter.onEndSearch();
        endSearchWatcher.assertExpected();

        HistogramWatcher historyDeletedWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Android.HistoryPage.QueryDuration")
                        .expectNoRecords("Android.HistoryPage.TimeToFirstVisibleContent")
                        .build();
        mAdapter.onHistoryDeleted();
        historyDeletedWatcher.assertExpected();
        verify(mViewTreeObserver).addOnPreDrawListener(any(OnPreDrawListener.class));
    }

    @Test
    public void testTimeToFirstVisibleContent_SupersededBySearchDoesNotRecord() {
        HistoryProvider mockProvider = Mockito.mock(HistoryProvider.class);
        mAdapter =
                new HistoryAdapter(
                        mContentManager,
                        mockProvider,
                        mHistorySyncPromoCoordinator,
                        /* shouldClusterByDomain= */ false,
                        /* snackbarManager= */ null,
                        /* profile= */ null);
        mAdapter.generateHeaderItemsForTest();
        mAdapter.onAttachedToRecyclerView(mRecyclerView);

        // Start initial load (remains in flight).
        mAdapter.startLoadingItems();

        // Supersede the in-flight initial load with a search query.
        mAdapter.search("query");

        Date today = new Date();
        ArrayList<HistoryItem> items = new ArrayList<>();
        items.add(StubbedHistoryProvider.createHistoryItem(0, today.getTime()));

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Android.HistoryPage.QueryDuration")
                        .expectNoRecords("Android.HistoryPage.TimeToFirstVisibleContent")
                        .build();

        mAdapter.onQueryHistoryComplete(items, /* hasMorePotentialMatches= */ false);

        watcher.assertExpected();
        verify(mViewTreeObserver, never()).addOnPreDrawListener(any(OnPreDrawListener.class));
    }

    @Test
    public void testHostNameFilterDoesNotRecordHistograms() {
        mAdapter.onAttachedToRecyclerView(mRecyclerView);
        mAdapter.setHostName("www.example.com");

        mHistoryProvider.setPaging(2);
        for (int i = 0; i < 5; ++i) {
            mHistoryProvider.addItem(
                    StubbedHistoryProvider.createHistoryItem(i, new Date().getTime()));
        }

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.HistoryPage.QueryDuration")
                        .expectNoRecords("Android.HistoryPage.TimeToFirstVisibleContent")
                        .build();

        mAdapter.startLoadingItems();
        mAdapter.loadMoreItems();

        watcher.assertExpected();
        verify(mViewTreeObserver, never()).addOnPreDrawListener(any(OnPreDrawListener.class));
    }

    @Test
    public void testCombinedFilters() {
        HistoryProvider mockProvider = Mockito.mock(HistoryProvider.class);
        mAdapter =
                new HistoryAdapter(
                        mContentManager,
                        mockProvider,
                        mHistorySyncPromoCoordinator,
                        /* shouldClusterByDomain= */ false,
                        /* snackbarManager= */ null,
                        /* profile= */ null);

        mAdapter.setAppId("com.example.app");
        mAdapter.setHostName("example.com");
        mAdapter.setClientIds(List.of("client_123"));
        mAdapter.search("test_query");

        Mockito.verify(mockProvider)
                .queryHistory(
                        "test_query",
                        new QueryOptions("com.example.app", "example.com", List.of("client_123")));
    }
}
