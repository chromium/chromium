// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.signin.signin_promo.SigninPromoCoordinator;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Unit tests for BrowsingHistoryBridge. */
@RunWith(BaseRobolectricTestRunner.class)
public class BrowsingHistoryBridgeTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock BrowsingHistoryBridge.Natives mNativeMocks;
    @Mock SigninPromoCoordinator mHistorySyncPromoCoordinator;

    @Mock private Profile mProfile;

    BrowsingHistoryBridge mBrowsingHistoryBridge;

    @Before
    public void setUp() {
        BrowsingHistoryBridgeJni.setInstanceForTesting(mNativeMocks);
        mBrowsingHistoryBridge = new BrowsingHistoryBridge(mProfile);
    }

    @Test
    public void testWhenDeletingBrowsingHistoryItems_MetricsEmitted() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Privacy.DeleteBrowsingData.Action",
                        DeleteBrowsingDataAction.HISTORY_PAGE_ENTRIES);
        mBrowsingHistoryBridge.removeItems();

        // Verify DeleteBrowsingDataAction metric is recorded.
        histogramWatcher.assertExpected();
    }

    @Test
    public void testAppIdPropagatesForDeletion() {
        // Ensure the app ID passed from BrowsingHistoryBridge is stored in the item
        // object, and later gets passed down when marking the item for removal.
        HistoryContentManager contentManager = mock(HistoryContentManager.class);
        HistoryAdapter adapter =
                new HistoryAdapter(
                        contentManager,
                        mBrowsingHistoryBridge,
                        mHistorySyncPromoCoordinator,
                        /* shouldClusterByDomain= */ false,
                        /* snackbarManager= */ null,
                        /* profile= */ null);
        mBrowsingHistoryBridge.setObserver(adapter);

        List<HistoryItem> items = new ArrayList<>();
        List<GURL> urls = List.of(GURL.emptyGURL());
        List<long[]> timestampsList = List.of(new long[0]);
        String appId = "org.chromium.dino.Trex";
        BrowsingHistoryBridge.createHistoryItemAndAddToList(
                items,
                GURL.emptyGURL(),
                "domain.com",
                "title",
                appId,
                0,
                urls,
                timestampsList,
                false,
                false);
        mBrowsingHistoryBridge.onQueryHistoryComplete(items, false);

        adapter.markItemForRemoval(items.get(0));
        verify(mNativeMocks).markItemForRemoval(anyLong(), any(), eq(appId), any());
    }

    @Test
    public void testCreateHistoryItemAndAddToList_SingleUrl() {
        List<HistoryItem> items = new ArrayList<>();
        GURL url = new GURL("https://example.com");
        long[] timestamps = new long[] {1000L, 2000L};
        List<GURL> urls = List.of(url);
        List<long[]> timestampsList = List.of(timestamps);

        BrowsingHistoryBridge.createHistoryItemAndAddToList(
                items,
                url,
                "example.com",
                "Example Title",
                /* appId= */ null,
                /* mostRecentJavaTimestamp= */ 2000L,
                urls,
                timestampsList,
                /* blockedVisit= */ false,
                /* isActorVisit= */ false);

        assertEquals(1, items.size());
        HistoryItem item = items.get(0);
        assertEquals(url, item.getUrl());
        assertArrayEquals(timestamps, item.getNativeTimestamps());

        Map<GURL, long[]> allTimestamps = item.getAllTimestamps();
        assertEquals(1, allTimestamps.size());
        assertArrayEquals(timestamps, allTimestamps.get(url));
    }

    @Test
    public void testCreateHistoryItemAndAddToList_MultipleUrlsGrouped() {
        List<HistoryItem> items = new ArrayList<>();
        GURL primaryUrl = new GURL("https://example.com/page1");
        GURL secondaryUrl = new GURL("https://example.com/page2");

        long[] primaryTimestamps = new long[] {100L, 200L};
        long[] secondaryTimestamps = new long[] {300L};

        List<GURL> urls = List.of(primaryUrl, secondaryUrl);
        List<long[]> timestampsList = List.of(primaryTimestamps, secondaryTimestamps);

        BrowsingHistoryBridge.createHistoryItemAndAddToList(
                items,
                primaryUrl,
                "example.com",
                "Example Grouped Title",
                /* appId= */ null,
                /* mostRecentJavaTimestamp= */ 300L,
                urls,
                timestampsList,
                /* blockedVisit= */ false,
                /* isActorVisit= */ false);

        assertEquals(1, items.size());
        HistoryItem item = items.get(0);
        assertEquals(primaryUrl, item.getUrl());
        assertArrayEquals(primaryTimestamps, item.getNativeTimestamps());

        Map<GURL, long[]> allTimestamps = item.getAllTimestamps();
        assertEquals(2, allTimestamps.size());
        assertArrayEquals(primaryTimestamps, allTimestamps.get(primaryUrl));
        assertArrayEquals(secondaryTimestamps, allTimestamps.get(secondaryUrl));
    }

    @Test
    public void testQueryHistoryWithOptions() {
        QueryOptions options = new QueryOptions("org.chromium.app", "example.com", "client_123");
        mBrowsingHistoryBridge.queryHistory("search query", options);

        verify(mNativeMocks)
                .queryHistory(
                        anyLong(),
                        any(),
                        eq("search query"),
                        eq("org.chromium.app"),
                        eq("example.com"),
                        eq("client_123"));
    }

    @Test
    public void testQueryClients() {
        mBrowsingHistoryBridge.queryClients();
        verify(mNativeMocks).getAllClients(anyLong());
    }
}
