// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
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
import org.chromium.chrome.browser.history.HistoryProvider.TimePeriodOption;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.signin.signin_promo.SigninPromoCoordinator;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;
import org.chromium.components.critical_actions.ActionType;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
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
                        /* shouldClusterByDomain= */ false);
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
                false,
                /* criticalActions= */ List.of());
        mBrowsingHistoryBridge.onQueryHistoryComplete(items, false);

        adapter.markItemForRemoval(items.get(0));
        verify(mNativeMocks).markItemForRemoval(anyLong(), any(), eq(appId), any());
    }

    @Test
    public void testMarkItemForRemoval_PassesTimestampsMap() {
        GURL url1 = new GURL("https://example.com/1");
        GURL url2 = new GURL("https://example.com/2");
        long[] timestamps1 = new long[] {100L, 200L};
        long[] timestamps2 = new long[] {300L};
        Map<GURL, long[]> allTimestamps = Map.of(url1, timestamps1, url2, timestamps2);

        HistoryItem item =
                new HistoryItem(
                        url1, "example.com", "Title", "appId", 300L, allTimestamps, false, false);

        mBrowsingHistoryBridge.markItemForRemoval(item);

        verify(mNativeMocks)
                .markItemForRemoval(
                        anyLong(),
                        eq(url1),
                        eq("appId"),
                        argThat(
                                map ->
                                        map != null
                                                && map.size() == 2
                                                && Arrays.equals(timestamps1, map.get(url1))
                                                && Arrays.equals(timestamps2, map.get(url2))));
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
                /* isActorVisit= */ false,
                /* criticalActions= */ List.of());

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
                /* isActorVisit= */ false,
                /* criticalActions= */ List.of());

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
    public void testCreateHistoryItemAndAddToList_PassesCriticalActions() {
        List<HistoryItem> items = new ArrayList<>();
        GURL url = new GURL("https://example.com");
        CriticalAction formFill =
                BrowsingHistoryBridge.createCriticalAction(
                        "id1", ActionType.FORM_FILL, "Filled a form", "Form tooltip");
        CriticalAction password =
                BrowsingHistoryBridge.createCriticalAction(
                        "id2",
                        ActionType.GOOGLE_PASSWORD_MANAGER,
                        "Used a password",
                        "Password tooltip");

        BrowsingHistoryBridge.createHistoryItemAndAddToList(
                items,
                url,
                "example.com",
                "Example Title",
                /* appId= */ null,
                /* mostRecentJavaTimestamp= */ 1000L,
                List.of(url),
                List.of(new long[] {1000L}),
                /* blockedVisit= */ false,
                /* isActorVisit= */ true,
                List.of(formFill, password));

        assertEquals(1, items.size());
        List<CriticalAction> actions = items.get(0).getCriticalActions();
        assertEquals(
                List.of(
                        new CriticalAction(
                                "id1", ActionType.FORM_FILL, "Filled a form", "Form tooltip"),
                        new CriticalAction(
                                "id2",
                                ActionType.GOOGLE_PASSWORD_MANAGER,
                                "Used a password",
                                "Password tooltip")),
                actions);
        assertEquals("id1", actions.get(0).getId());
        assertEquals(1, actions.get(0).getActionType());
        assertEquals("Filled a form", actions.get(0).getLabel());
        assertEquals("Form tooltip", actions.get(0).getTooltip());
    }

    @Test
    public void testHistoryItem_CriticalActionsDefaultToEmpty() {
        GURL url = new GURL("https://example.com");
        HistoryItem item =
                new HistoryItem(
                        url,
                        "example.com",
                        "Title",
                        /* appId= */ null,
                        1000L,
                        new long[] {1000L},
                        /* blockedVisit= */ false,
                        /* isActorVisit= */ false);

        assertTrue(item.getCriticalActions().isEmpty());
    }

    @Test
    public void testHistoryItem_CriticalActionsDefensiveCopy() {
        GURL url = new GURL("https://example.com");
        List<CriticalAction> mutableActions = new ArrayList<>();
        mutableActions.add(new CriticalAction("id", ActionType.FORM_FILL, "label", "tooltip"));

        HistoryItem item =
                new HistoryItem(
                        url,
                        "example.com",
                        "Title",
                        /* appId= */ null,
                        1000L,
                        Map.of(url, new long[] {1000L}),
                        /* blockedVisit= */ false,
                        /* isActorVisit= */ true,
                        mutableActions);

        mutableActions.clear();
        List<CriticalAction> actions = item.getCriticalActions();
        assertEquals(
                List.of(new CriticalAction("id", ActionType.FORM_FILL, "label", "tooltip")),
                actions);
        assertThrows(UnsupportedOperationException.class, actions::clear);
    }

    @Test
    public void testHistoryItem_CriticalActionsPreservedByBuilder() {
        GURL url = new GURL("https://example.com");
        CriticalAction action = new CriticalAction("id", ActionType.FORM_FILL, "label", "tooltip");
        HistoryItem item =
                new HistoryItem(
                        url,
                        "example.com",
                        "Title",
                        /* appId= */ null,
                        1000L,
                        Map.of(url, new long[] {1000L}),
                        /* blockedVisit= */ false,
                        /* isActorVisit= */ true,
                        List.of(action));

        HistoryItem copy = item.toBuilder().setIsExpanded(true).build();

        assertEquals(
                List.of(new CriticalAction("id", ActionType.FORM_FILL, "label", "tooltip")),
                copy.getCriticalActions());
    }

    @Test
    public void testCriticalAction_EqualsAndHashCode() {
        CriticalAction action =
                new CriticalAction("id1", ActionType.FORM_FILL, "label1", "tooltip1");
        CriticalAction same = new CriticalAction("id1", ActionType.FORM_FILL, "label1", "tooltip1");

        assertEquals(action, same);
        assertEquals(action.hashCode(), same.hashCode());

        assertNotEquals(
                action, new CriticalAction("id2", ActionType.FORM_FILL, "label1", "tooltip1"));
        assertNotEquals(
                action, new CriticalAction("id1", ActionType.DOWNLOAD, "label1", "tooltip1"));
        assertNotEquals(
                action, new CriticalAction("id1", ActionType.FORM_FILL, "label2", "tooltip1"));
        assertNotEquals(
                action, new CriticalAction("id1", ActionType.FORM_FILL, "label1", "tooltip2"));
        assertNotEquals(action, null);
        assertNotEquals(action, new Object());
    }

    @Test
    public void testQueryHistoryWithOptions() {
        // Test all combinations of `includeUserVisits` and `includeActorVisits` bits.
        boolean[] vals = {true, true, false, false, true};
        for (int i = 0; i + 1 < vals.length; i++) {
            QueryOptions options =
                    new QueryOptions(
                            "org.chromium.app",
                            "example.com",
                            List.of("client_123", "client_456"),
                            vals[i],
                            vals[i + 1],
                            /* beginTimeMs= */ QueryOptions.UNBOUNDED_TIME_MS,
                            /* endTimeMs= */ QueryOptions.UNBOUNDED_TIME_MS);
            mBrowsingHistoryBridge.queryHistory("search query", options);

            verify(mNativeMocks)
                    .queryHistory(
                            anyLong(),
                            any(),
                            eq("search query"),
                            eq("org.chromium.app"),
                            eq("example.com"),
                            eq(List.of("client_123", "client_456")),
                            eq(vals[i]),
                            eq(vals[i + 1]),
                            eq(QueryOptions.UNBOUNDED_TIME_MS),
                            eq(QueryOptions.UNBOUNDED_TIME_MS));
        }
    }

    @Test
    public void testQueryHistoryWithTimePeriod() {
        QueryOptions options =
                new QueryOptions(
                        /* appId= */ null,
                        /* hostName= */ null,
                        List.of(),
                        /* includeUserVisits= */ true,
                        /* includeActorVisits= */ true,
                        /* beginTimeMs= */ 1000L,
                        /* endTimeMs= */ 2000L);
        mBrowsingHistoryBridge.queryHistory("", options);

        verify(mNativeMocks)
                .queryHistory(
                        anyLong(),
                        any(),
                        eq(""),
                        eq(null),
                        eq(null),
                        eq(List.of()),
                        eq(true),
                        eq(true),
                        eq(1000L),
                        eq(2000L));
    }

    @Test
    public void testQueryClients() {
        mBrowsingHistoryBridge.queryClients();
        verify(mNativeMocks).getAllClients(anyLong());
    }

    @Test
    public void testQueryTimePeriodOptions() {
        HistoryProvider.BrowsingHistoryObserver observer =
                mock(HistoryProvider.BrowsingHistoryObserver.class);
        mBrowsingHistoryBridge.setObserver(observer);
        List<TimePeriodOption> options =
                List.of(
                        BrowsingHistoryBridge.createTimePeriodOption(
                                TimePeriodOptionId.ALL_TIME, "All time", 0L, 0L),
                        BrowsingHistoryBridge.createTimePeriodOption(
                                TimePeriodOptionId.YESTERDAY, "Yesterday", 1000L, 2000L));
        doReturn(options).when(mNativeMocks).getTimePeriodOptions();

        mBrowsingHistoryBridge.queryTimePeriodOptions();

        verify(mNativeMocks).getTimePeriodOptions();
        verify(observer).onQueryTimePeriodOptionsComplete(options);
        assertEquals(TimePeriodOptionId.YESTERDAY, options.get(1).id);
        assertEquals("Yesterday", options.get(1).label);
        assertEquals(1000L, options.get(1).beginTimeMs);
        assertEquals(2000L, options.get(1).endTimeMs);
    }
}
