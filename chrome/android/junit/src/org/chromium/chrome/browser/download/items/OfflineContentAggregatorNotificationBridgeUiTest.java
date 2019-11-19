// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.items;

import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.ArgumentMatchers;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadNotifier;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.OfflineItemVisuals;
import org.chromium.components.offline_items_collection.PendingState;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for {@link OfflineContentAggregatorNotifierBridgeUi}.  Validate that it interacts with
 * both the {@link DownloadNotifier} and the {@link OfflineContentProvider} in expected ways.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OfflineContentAggregatorNotificationBridgeUiTest {
    /** Helper class to validate that a DownloadInfo has the right ContentId. */
    static class DownloadInfoIdMatcher implements ArgumentMatcher<DownloadInfo> {
        private final ContentId mExpectedId;

        public DownloadInfoIdMatcher(ContentId expected) {
            mExpectedId = expected;
        }

        @Override
        public boolean matches(DownloadInfo argument) {
            return ((DownloadInfo) argument).getContentId().equals(mExpectedId);
        }

        @Override
        public String toString() {
            return mExpectedId == null ? null : mExpectedId.toString();
        }
    }

    @Mock
    private OfflineContentProvider mProvider;

    @Mock
    private DownloadNotifier mNotifier;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static OfflineItem buildOfflineItem(ContentId id, @OfflineItemState int state) {
        OfflineItem item = new OfflineItem();
        item.id = id;
        item.state = state;
        return item;
    }

    @Test
    public void testAddedItemsGetSentToTheUi() {
        OfflineContentAggregatorNotificationBridgeUi bridge =
                new OfflineContentAggregatorNotificationBridgeUi(mProvider, mNotifier);
        verify(mProvider, times(1)).addObserver(bridge);

        ArrayList<OfflineItem> items = new ArrayList<OfflineItem>() {
            {
                add(buildOfflineItem(new ContentId("1", "A"), OfflineItemState.IN_PROGRESS));
                add(buildOfflineItem(new ContentId("2", "B"), OfflineItemState.PENDING));
                add(buildOfflineItem(new ContentId("3", "C"), OfflineItemState.COMPLETE));
                add(buildOfflineItem(new ContentId("4", "D"), OfflineItemState.CANCELLED));
                add(buildOfflineItem(new ContentId("5", "E"), OfflineItemState.INTERRUPTED));
                add(buildOfflineItem(new ContentId("6", "F"), OfflineItemState.FAILED));
                add(buildOfflineItem(new ContentId("7", "G"), OfflineItemState.PAUSED));
            }
        };

        bridge.onItemsAdded(items);

        verify(mProvider, times(1)).getVisualsForItem(items.get(0).id, bridge);
        verify(mProvider, times(1)).getVisualsForItem(items.get(1).id, bridge);
        verify(mProvider, times(1)).getVisualsForItem(items.get(2).id, bridge);
        verify(mProvider, never()).getVisualsForItem(items.get(3).id, bridge);
        verify(mProvider, times(1)).getVisualsForItem(items.get(4).id, bridge);
        verify(mProvider, times(1)).getVisualsForItem(items.get(5).id, bridge);
        verify(mProvider, times(1)).getVisualsForItem(items.get(6).id, bridge);

        for (int i = 0; i < items.size(); i++) {
            bridge.onVisualsAvailable(items.get(i).id, new OfflineItemVisuals());
        }

        verify(mNotifier, times(1))
                .notifyDownloadProgress(argThat(new DownloadInfoIdMatcher(items.get(0).id)),
                        ArgumentMatchers.anyLong(), ArgumentMatchers.anyBoolean());
        verify(mNotifier, times(1))
                .notifyDownloadSuccessful(argThat(new DownloadInfoIdMatcher(items.get(2).id)),
                        ArgumentMatchers.anyLong(), ArgumentMatchers.anyBoolean(),
                        ArgumentMatchers.anyBoolean());
        verify(mNotifier, times(1))
                .notifyDownloadCanceled(items.get(3).id /* OfflineItemState.CANCELLED */);
        verify(mNotifier, times(1))
                .notifyDownloadInterrupted(argThat(new DownloadInfoIdMatcher(items.get(4).id)),
                        ArgumentMatchers.anyBoolean(), eq(PendingState.NOT_PENDING));
        verify(mNotifier, times(1))
                .notifyDownloadFailed(argThat(new DownloadInfoIdMatcher(items.get(5).id)));
        verify(mNotifier, times(1))
                .notifyDownloadPaused(argThat(new DownloadInfoIdMatcher(items.get(6).id)));

        bridge.destroy();
        verify(mProvider, times(1)).removeObserver(bridge);
    }

    @Test
    public void testItemUpdatesGetSentToTheUi() {
        OfflineContentAggregatorNotificationBridgeUi bridge =
                new OfflineContentAggregatorNotificationBridgeUi(mProvider, mNotifier);
        verify(mProvider, times(1)).addObserver(bridge);

        ArrayList<OfflineItem> items = new ArrayList<OfflineItem>() {
            {
                add(buildOfflineItem(new ContentId("1", "A"), OfflineItemState.IN_PROGRESS));
                add(buildOfflineItem(new ContentId("2", "B"), OfflineItemState.PENDING));
                add(buildOfflineItem(new ContentId("3", "C"), OfflineItemState.COMPLETE));
                add(buildOfflineItem(new ContentId("4", "D"), OfflineItemState.CANCELLED));
                add(buildOfflineItem(new ContentId("5", "E"), OfflineItemState.INTERRUPTED));
                add(buildOfflineItem(new ContentId("6", "F"), OfflineItemState.FAILED));
                add(buildOfflineItem(new ContentId("7", "G"), OfflineItemState.PAUSED));
            }
        };

        for (int i = 0; i < items.size(); i++) bridge.onItemUpdated(items.get(i), null);

        verify(mProvider, times(1)).getVisualsForItem(items.get(0).id, bridge);
        verify(mProvider, times(1)).getVisualsForItem(items.get(1).id, bridge);
        verify(mProvider, times(1)).getVisualsForItem(items.get(2).id, bridge);
        verify(mProvider, never()).getVisualsForItem(items.get(3).id, bridge);
        verify(mProvider, times(1)).getVisualsForItem(items.get(4).id, bridge);
        verify(mProvider, times(1)).getVisualsForItem(items.get(5).id, bridge);
        verify(mProvider, times(1)).getVisualsForItem(items.get(6).id, bridge);

        for (int i = 0; i < items.size(); i++) {
            bridge.onVisualsAvailable(items.get(i).id, new OfflineItemVisuals());
        }

        verify(mNotifier, times(1))
                .notifyDownloadProgress(argThat(new DownloadInfoIdMatcher(items.get(0).id)),
                        ArgumentMatchers.anyLong(), ArgumentMatchers.anyBoolean());
        verify(mNotifier, times(1))
                .notifyDownloadSuccessful(argThat(new DownloadInfoIdMatcher(items.get(2).id)),
                        ArgumentMatchers.anyLong(), ArgumentMatchers.anyBoolean(),
                        ArgumentMatchers.anyBoolean());
        verify(mNotifier, times(1))
                .notifyDownloadCanceled(items.get(3).id /* OfflineItemState.CANCELLED */);
        verify(mNotifier, times(1))
                .notifyDownloadInterrupted(argThat(new DownloadInfoIdMatcher(items.get(4).id)),
                        ArgumentMatchers.anyBoolean(), eq(PendingState.NOT_PENDING));
        verify(mNotifier, times(1))
                .notifyDownloadFailed(argThat(new DownloadInfoIdMatcher(items.get(5).id)));
        verify(mNotifier, times(1))
                .notifyDownloadPaused(argThat(new DownloadInfoIdMatcher(items.get(6).id)));

        bridge.destroy();
        verify(mProvider, times(1)).removeObserver(bridge);
    }

    @Test
    public void testNullVisuals() {
        OfflineContentAggregatorNotificationBridgeUi bridge =
                new OfflineContentAggregatorNotificationBridgeUi(mProvider, mNotifier);
        verify(mProvider, times(1)).addObserver(bridge);

        OfflineItem item1 = buildOfflineItem(new ContentId("1", "A"), OfflineItemState.IN_PROGRESS);
        OfflineItem item2 = buildOfflineItem(new ContentId("2", "B"), OfflineItemState.IN_PROGRESS);

        OfflineItemVisuals visuals1 = new OfflineItemVisuals();
        visuals1.icon = Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8);

        bridge.onItemUpdated(item1, null);
        bridge.onItemUpdated(item2, null);

        verify(mProvider, times(1)).getVisualsForItem(item1.id, bridge);
        verify(mProvider, times(1)).getVisualsForItem(item2.id, bridge);

        ArgumentCaptor<DownloadInfo> captor = ArgumentCaptor.forClass(DownloadInfo.class);

        bridge.onVisualsAvailable(item1.id, visuals1);
        bridge.onVisualsAvailable(item2.id, null);
        verify(mNotifier, times(2))
                .notifyDownloadProgress(captor.capture(), ArgumentMatchers.anyLong(),
                        ArgumentMatchers.anyBoolean());

        List<DownloadInfo> capturedInfo = captor.getAllValues();
        Assert.assertEquals(item1.id, capturedInfo.get(0).getContentId());
        Assert.assertEquals(visuals1.icon, capturedInfo.get(0).getIcon());
        Assert.assertEquals(item2.id, capturedInfo.get(1).getContentId());
        Assert.assertEquals(null, capturedInfo.get(1).getIcon());

        bridge.destroy();
        verify(mProvider, times(1)).removeObserver(bridge);
    }

    @Test
    public void testRemovedItemsGetRemovedFromTheUi() {
        OfflineContentAggregatorNotificationBridgeUi bridge =
                new OfflineContentAggregatorNotificationBridgeUi(mProvider, mNotifier);
        verify(mProvider, times(1)).addObserver(bridge);

        ContentId id = new ContentId("1", "A");

        bridge.onItemRemoved(id);
        verify(mNotifier, times(1)).notifyDownloadCanceled(id);

        bridge.destroy();
        verify(mProvider, times(1)).removeObserver(bridge);
    }

    @Test
    public void testRemovedItemsIgnoreVisualsCallback() {
        OfflineContentAggregatorNotificationBridgeUi bridge =
                new OfflineContentAggregatorNotificationBridgeUi(mProvider, mNotifier);
        verify(mProvider, times(1)).addObserver(bridge);

        OfflineItem item = buildOfflineItem(new ContentId("1", "A"), OfflineItemState.IN_PROGRESS);

        bridge.onItemUpdated(item, null);
        verify(mProvider, times(1)).getVisualsForItem(item.id, bridge);

        bridge.onItemRemoved(item.id);
        bridge.onVisualsAvailable(item.id, new OfflineItemVisuals());
        InOrder order = inOrder(mNotifier);
        order.verify(mNotifier, times(1)).notifyDownloadCanceled(item.id);
        order.verifyNoMoreInteractions();

        bridge.destroy();
        verify(mProvider, times(1)).removeObserver(bridge);
    }

    @Test
    public void testOnlyRequestsVisualsOnceForMultipleUpdates() {
        OfflineContentAggregatorNotificationBridgeUi bridge =
                new OfflineContentAggregatorNotificationBridgeUi(mProvider, mNotifier);
        verify(mProvider, times(1)).addObserver(bridge);

        OfflineItem item = buildOfflineItem(new ContentId("1", "A"), OfflineItemState.IN_PROGRESS);

        bridge.onItemUpdated(item, null);
        bridge.onItemUpdated(item, null);
        verify(mProvider, times(1)).getVisualsForItem(item.id, bridge);

        bridge.destroy();
        verify(mProvider, times(1)).removeObserver(bridge);
    }

    @Test
    public void testVisualsAreCachedForInterestingItems() {
        OfflineContentAggregatorNotificationBridgeUi bridge =
                new OfflineContentAggregatorNotificationBridgeUi(mProvider, mNotifier);
        verify(mProvider, times(1)).addObserver(bridge);

        ArrayList<OfflineItem> interestingItems = new ArrayList<OfflineItem>() {
            {
                add(buildOfflineItem(new ContentId("1", "A"), OfflineItemState.IN_PROGRESS));
                add(buildOfflineItem(new ContentId("2", "B"), OfflineItemState.PENDING));
                add(buildOfflineItem(new ContentId("3", "C"), OfflineItemState.COMPLETE));
                add(buildOfflineItem(new ContentId("5", "E"), OfflineItemState.INTERRUPTED));
                add(buildOfflineItem(new ContentId("7", "G"), OfflineItemState.PAUSED));
            }
        };

        ArrayList<OfflineItem> uninterestingItems = new ArrayList<OfflineItem>() {
            {
                add(buildOfflineItem(new ContentId("6", "F"), OfflineItemState.FAILED));
            }
        };

        for (int i = 0; i < interestingItems.size(); i++) {
            OfflineItem item = interestingItems.get(i);
            bridge.onItemUpdated(item, null);
            bridge.onVisualsAvailable(item.id, null);
            bridge.onItemUpdated(item, null);
            verify(mProvider, times(1)).getVisualsForItem(item.id, bridge);
            verify(mNotifier, times(2))
                    .notifyDownloadProgress(ArgumentMatchers.any(), ArgumentMatchers.anyLong(),
                            ArgumentMatchers.anyBoolean());
        }

        for (int i = 0; i < uninterestingItems.size(); i++) {
            OfflineItem item = uninterestingItems.get(i);
            bridge.onItemUpdated(item, null);
            bridge.onVisualsAvailable(item.id, null);
            bridge.onItemUpdated(item, null);
            verify(mProvider, times(2)).getVisualsForItem(item.id, bridge);
        }

        bridge.destroy();
        verify(mProvider, times(1)).removeObserver(bridge);
    }

    @Test
    public void testVisualsGetClearedForUninterestingItems() {
        OfflineContentAggregatorNotificationBridgeUi bridge =
                new OfflineContentAggregatorNotificationBridgeUi(mProvider, mNotifier);
        verify(mProvider, times(1)).addObserver(bridge);

        ContentId id = new ContentId("1", "A");
        OfflineItem item1 = buildOfflineItem(id, OfflineItemState.IN_PROGRESS);
        OfflineItem item2 = buildOfflineItem(id, OfflineItemState.FAILED);
        OfflineItem item3 = buildOfflineItem(id, OfflineItemState.IN_PROGRESS);

        bridge.onItemUpdated(item1, null);
        bridge.onVisualsAvailable(item1.id, new OfflineItemVisuals());
        bridge.onItemUpdated(item2, null);
        bridge.onItemUpdated(item3, null);
        bridge.onVisualsAvailable(item1.id, new OfflineItemVisuals());
        verify(mProvider, times(2)).getVisualsForItem(id, bridge);

        bridge.destroy();
        verify(mProvider, times(1)).removeObserver(bridge);
    }
}
