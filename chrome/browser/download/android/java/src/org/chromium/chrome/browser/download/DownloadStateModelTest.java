// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.junit.Assert.assertEquals;
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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link DownloadStateModel}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DownloadStateModelTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

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
    public void testSnapshot_seedsOnlyActiveItemsAsNewItems() {
        OfflineItem active = createItem("active", OfflineItemState.IN_PROGRESS);
        onAllItemsRetrieved(
                createItem("done", OfflineItemState.COMPLETE),
                active,
                createItem("cancelled", OfflineItemState.CANCELLED));
        assertEquals(List.of(active), activeItems());
        verify(mObserver).onItemChanged(active, null);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    public void testSnapshot_doesNotResurrectItemsReportedBeforeIt() {
        // Callbacks that arrive before the (older) snapshot are fresher and win.
        mModel.onItemUpdated(createItem("completed", OfflineItemState.COMPLETE), null);
        mModel.onItemRemoved(id("removed"));
        onAllItemsRetrieved(
                createItem("completed", OfflineItemState.IN_PROGRESS),
                createItem("removed", OfflineItemState.IN_PROGRESS));
        assertTrue(activeItems().isEmpty());
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
        mModel.destroy();
        verify(mProvider).removeObserver(mModel);
        assertTrue(activeItems().isEmpty());

        // The snapshot request is still outstanding; its late result must be dropped.
        onAllItemsRetrieved(createItem("b", OfflineItemState.IN_PROGRESS));
        assertTrue(activeItems().isEmpty());
    }
}
