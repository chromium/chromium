// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.CollectionUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;
import java.util.Collections;

/** Unit tests for the DeleteUndoOfflineItemFilter class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DeleteUndoOfflineItemFilterTest {
    @Mock private OfflineItemFilterSource mSource;

    @Mock private OfflineItemFilterObserver mObserver;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    public void testFiltering() {
        ContentId id1 = new ContentId("test", "1");
        ContentId id2 = new ContentId("test", "2");
        ContentId id3 = new ContentId("test", "3");

        OfflineItem item1 = buildItem(id1);
        OfflineItem item2 = buildItem(id2);
        OfflineItem item3 = buildItem(id3);
        Collection<OfflineItem> sourceItems = CollectionUtil.newHashSet(item1, item2, item3);
        when(mSource.getItems()).thenReturn(sourceItems);

        DeleteUndoOfflineItemFilter filter = new DeleteUndoOfflineItemFilter(mSource);
        filter.addObserver(mObserver);
        Assert.assertEquals(sourceItems, filter.getItems());

        // Test removing items.
        filter.addPendingDeletions(CollectionUtil.newHashSet(item1, item2));
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item1, item2));
        Assert.assertEquals(CollectionUtil.newHashSet(item3), filter.getItems());

        // Test removing more items.
        filter.addPendingDeletions(CollectionUtil.newHashSet(item3));
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item3));
        Assert.assertEquals(Collections.emptySet(), filter.getItems());

        // Test undoing items across removals.
        filter.removePendingDeletions(CollectionUtil.newHashSet(item1, item3));
        verify(mObserver, times(1)).onItemsAdded(CollectionUtil.newHashSet(item1, item3));
        Assert.assertEquals(CollectionUtil.newHashSet(item1, item3), filter.getItems());

        // Test undoing another removal.
        filter.removePendingDeletions(CollectionUtil.newHashSet(item2));
        verify(mObserver, times(1)).onItemsAdded(CollectionUtil.newHashSet(item2));
        Assert.assertEquals(sourceItems, filter.getItems());

        // Test removing a pending item has no effect.
        filter.addPendingDeletions(CollectionUtil.newHashSet(item1));
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item1));

        sourceItems.remove(item1);
        filter.onItemsRemoved(CollectionUtil.newHashSet(item1));

        // Test adding and removing an item that is not in the list.
        ContentId id4 = new ContentId("test", "4");
        OfflineItem item4 = buildItem(id4);
        filter.addPendingDeletions(CollectionUtil.newHashSet(item4));
        filter.removePendingDeletions(CollectionUtil.newHashSet(item4));

        verifyNoMoreInteractions(mObserver);
    }

    private static OfflineItem buildItem(ContentId id) {
        OfflineItem item = new OfflineItem();
        item.id = id;
        return item;
    }
}
