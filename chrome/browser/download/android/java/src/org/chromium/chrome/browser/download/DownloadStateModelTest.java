// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;

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
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link DownloadStateModel}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DownloadStateModelTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    @Mock private OfflineContentProvider mProvider;
    @Mock private DownloadStateModel.Observer mObserver;
    @Captor private ArgumentCaptor<Callback<ArrayList<OfflineItem>>> mGetAllItemsCallbackCaptor;

    private DownloadStateModel mModel;

    @Before
    public void setUp() {
        mModel = new DownloadStateModel(mProvider);
        verify(mProvider).addObserver(mModel);
        verify(mProvider).getAllItems(mGetAllItemsCallbackCaptor.capture());
        mModel.addObserver(mObserver);
    }

    private static ContentId id(String guid) {
        return LegacyHelpers.buildLegacyContentId(/* isOfflinePage= */ false, guid);
    }

    private static OfflineItem createItem(String guid, @OfflineItemState int state) {
        OfflineItem item = new OfflineItem();
        item.id = id(guid);
        item.state = state;
        return item;
    }

    /** Creates a completed item that started {@code startedAgoMs} before now. */
    private static OfflineItem createCompletedItem(String guid, long startedAgoMs) {
        OfflineItem item = createItem(guid, OfflineItemState.COMPLETE);
        long nowMs = TimeUtils.currentTimeMillis();
        item.creationTimeMs = nowMs - startedAgoMs;
        item.completionTimeMs = nowMs;
        return item;
    }

    private static List<ContentId> ids(List<OfflineItem> items) {
        List<ContentId> ids = new ArrayList<>(items.size());
        for (OfflineItem item : items) {
            ids.add(item.id);
        }
        return ids;
    }

    private void onAllItemsRetrieved(OfflineItem... items) {
        mGetAllItemsCallbackCaptor.getValue().onResult(new ArrayList<>(List.of(items)));
    }

    private List<OfflineItem> activeItems() {
        return new ArrayList<>(mModel.getActiveItems());
    }

    @Test
    public void testItemLifecycle_cachesLatestActiveDataAndReportsPrevious() {
        OfflineItem started = createItem("a", OfflineItemState.IN_PROGRESS);
        mModel.onItemsAdded(List.of(started));
        verify(mObserver).onItemChanged(started, null);
        assertEquals(List.of(started), activeItems());

        OfflineItem paused = createItem("a", OfflineItemState.PAUSED);
        mModel.onItemUpdated(paused, /* updateDelta= */ null);
        verify(mObserver).onItemChanged(paused, started);
        assertEquals("Updates replace the cached data", List.of(paused), activeItems());

        OfflineItem completed = createItem("a", OfflineItemState.COMPLETE);
        mModel.onItemUpdated(completed, /* updateDelta= */ null);
        verify(mObserver).onItemChanged(completed, paused);
        assertTrue("Terminal items leave the cache", activeItems().isEmpty());

        mModel.onItemRemoved(id("a"));
        verify(mObserver).onItemRemoved(id("a"), null);
    }

    @Test
    public void testActiveItemRemoved_reportsPrevious() {
        OfflineItem item = createItem("a", OfflineItemState.PENDING);
        mModel.onItemsAdded(List.of(item));

        mModel.onItemRemoved(id("a"));
        verify(mObserver).onItemRemoved(id("a"), item);
        assertTrue(activeItems().isEmpty());
    }

    @Test
    public void testUntrackableItems_areIgnored() {
        OfflineItem transientItem = createItem("transient", OfflineItemState.IN_PROGRESS);
        transientItem.isTransient = true;
        OfflineItem suggestedItem = createItem("suggested", OfflineItemState.IN_PROGRESS);
        suggestedItem.isSuggested = true;
        OfflineItem noIdItem = createItem("x", OfflineItemState.IN_PROGRESS);
        noIdItem.id = null;

        mModel.onItemsAdded(List.of(transientItem, suggestedItem, noIdItem));
        onAllItemsRetrieved(transientItem, suggestedItem, noIdItem);
        assertTrue(activeItems().isEmpty());
        verifyNoInteractions(mObserver);
    }

    @Test
    public void testSnapshot_seedsActiveItemsAsNewAndCompletedOnesSilently() {
        OfflineItem active = createItem("active", OfflineItemState.IN_PROGRESS);
        OfflineItem done = createCompletedItem("done", /* startedAgoMs= */ 0);
        onAllItemsRetrieved(done, active, createItem("cancelled", OfflineItemState.CANCELLED));
        assertEquals(List.of(active), activeItems());
        assertEquals(List.of(done), mModel.getRecentCompletedItems());
        verify(mObserver).onItemChanged(active, null);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    public void testSnapshot_doesNotResurrectItemsReportedBeforeIt() {
        // Callbacks that arrive before the (older) snapshot are fresher and win.
        mModel.onItemUpdated(
                createCompletedItem("completed", /* startedAgoMs= */ 0), /* updateDelta= */ null);
        mModel.onItemRemoved(id("removed"));
        onAllItemsRetrieved(
                createItem("completed", OfflineItemState.IN_PROGRESS),
                createItem("removed", OfflineItemState.IN_PROGRESS));
        assertTrue(activeItems().isEmpty());
        assertEquals(List.of(id("completed")), ids(mModel.getRecentCompletedItems()));
    }

    @Test
    public void testRemoveObserver_stopsCallbacks() {
        mModel.removeObserver(mObserver);
        mModel.onItemsAdded(List.of(createItem("a", OfflineItemState.IN_PROGRESS)));
        mModel.onItemRemoved(id("a"));
        onAllItemsRetrieved(createItem("b", OfflineItemState.IN_PROGRESS));
        verifyNoInteractions(mObserver);
    }

    @Test
    public void testDestroy_unregistersAndCancelsPendingSnapshot() {
        mModel.onItemsAdded(List.of(createItem("a", OfflineItemState.IN_PROGRESS)));
        mModel.onItemUpdated(
                createCompletedItem("b", /* startedAgoMs= */ 0), /* updateDelta= */ null);
        mModel.destroy();
        verify(mProvider).removeObserver(mModel);
        assertTrue(activeItems().isEmpty());
        assertTrue(mModel.getRecentCompletedItems().isEmpty());

        // The snapshot request is still outstanding; its late result must be dropped.
        onAllItemsRetrieved(
                createItem("c", OfflineItemState.IN_PROGRESS),
                createCompletedItem("d", /* startedAgoMs= */ 0));
        assertTrue(activeItems().isEmpty());
        assertTrue(mModel.getRecentCompletedItems().isEmpty());
    }

    @Test
    public void testRecentCompletedItems_withinWindowMostRecentFirst() {
        onAllItemsRetrieved(
                createCompletedItem("older", TimeUnit.HOURS.toMillis(3)),
                createCompletedItem("newest", TimeUnit.MINUTES.toMillis(5)),
                createCompletedItem("tooOld", DownloadStateModel.RECENT_COMPLETED_WINDOW_MS + 1),
                createCompletedItem("middle", TimeUnit.HOURS.toMillis(1)));
        assertEquals(
                List.of(id("newest"), id("middle"), id("older")),
                ids(mModel.getRecentCompletedItems()));

        // Items age out as time passes.
        mFakeTime.advanceMillis(
                DownloadStateModel.RECENT_COMPLETED_WINDOW_MS - TimeUnit.HOURS.toMillis(2));
        assertEquals(List.of(id("newest"), id("middle")), ids(mModel.getRecentCompletedItems()));
    }

    @Test
    public void testRecentCompletedItems_cappedToNewest() {
        int extra = 3;
        OfflineItem[] items =
                new OfflineItem[DownloadStateModel.MAX_RECENT_COMPLETED_ITEMS + extra];
        for (int i = 0; i < items.length; i++) {
            // Item 0 is the oldest, so it and the next few are the ones dropped.
            items[i] = createCompletedItem("item" + i, TimeUnit.MINUTES.toMillis(items.length - i));
        }
        onAllItemsRetrieved(items);

        List<ContentId> recent = ids(mModel.getRecentCompletedItems());
        assertEquals(DownloadStateModel.MAX_RECENT_COMPLETED_ITEMS, recent.size());
        assertEquals(id("item" + (items.length - 1)), recent.get(0));
        assertEquals(id("item" + extra), recent.get(recent.size() - 1));
    }

    @Test
    public void testRecentCompletedItems_followSessionChanges() {
        mModel.onItemsAdded(
                List.of(
                        createItem("a", OfflineItemState.IN_PROGRESS),
                        createItem("b", OfflineItemState.IN_PROGRESS)));
        // Only successful completions are listed.
        mModel.onItemUpdated(
                createCompletedItem("a", /* startedAgoMs= */ 0), /* updateDelta= */ null);
        mModel.onItemUpdated(createItem("b", OfflineItemState.FAILED), /* updateDelta= */ null);
        assertEquals(List.of(id("a")), ids(mModel.getRecentCompletedItems()));

        // Later updates replace the data, e.g. after a rename.
        OfflineItem renamed = createCompletedItem("a", /* startedAgoMs= */ 0);
        renamed.title = "renamed";
        mModel.onItemUpdated(renamed, /* updateDelta= */ null);
        assertEquals("renamed", mModel.getRecentCompletedItems().get(0).title);

        // A completion that becomes active again (e.g. re-downloaded) leaves the list until it
        // completes again; a removed one leaves it for good.
        mModel.onItemUpdated(
                createItem("a", OfflineItemState.IN_PROGRESS), /* updateDelta= */ null);
        assertTrue(mModel.getRecentCompletedItems().isEmpty());
        mModel.onItemUpdated(
                createCompletedItem("a", /* startedAgoMs= */ 0), /* updateDelta= */ null);
        mModel.onItemRemoved(id("a"));
        assertTrue(mModel.getRecentCompletedItems().isEmpty());
    }

    @Test
    public void testIsVisibleInWindow() {
        OfflineItem regular = createItem("regular", OfflineItemState.IN_PROGRESS);
        OfflineItem incognito = createItem("incognito", OfflineItemState.IN_PROGRESS);
        incognito.isOffTheRecord = true;
        incognito.otrProfileId = OtrProfileId.serialize(OtrProfileId.getPrimaryOtrProfileId());
        OfflineItem customTab = createItem("cct", OfflineItemState.IN_PROGRESS);
        customTab.isOffTheRecord = true;
        customTab.otrProfileId = OtrProfileId.serialize(new OtrProfileId("CCT:Incognito-1"));
        OfflineItem unknownProfile = createItem("unknown", OfflineItemState.IN_PROGRESS);
        unknownProfile.isOffTheRecord = true;

        // Regular downloads show everywhere; incognito ones only with incognito tabs, and only
        // from the primary off-the-record profile those tabs use.
        assertTrue(DownloadStateModel.isVisibleInWindow(regular, /* isIncognitoSelected= */ false));
        assertTrue(DownloadStateModel.isVisibleInWindow(regular, /* isIncognitoSelected= */ true));
        assertFalse(
                DownloadStateModel.isVisibleInWindow(incognito, /* isIncognitoSelected= */ false));
        assertTrue(
                DownloadStateModel.isVisibleInWindow(incognito, /* isIncognitoSelected= */ true));
        assertFalse(
                DownloadStateModel.isVisibleInWindow(customTab, /* isIncognitoSelected= */ true));
        assertFalse(
                DownloadStateModel.isVisibleInWindow(
                        unknownProfile, /* isIncognitoSelected= */ true));
    }
}
