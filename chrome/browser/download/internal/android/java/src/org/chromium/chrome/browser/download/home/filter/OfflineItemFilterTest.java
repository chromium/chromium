// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
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
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

/** Unit tests for the OfflineItemFilter class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OfflineItemFilterTest {
    private class OfflineItemFilterImpl extends OfflineItemFilter {
        private final Set<OfflineItem> mFilteredItems = new HashSet<>();

        public OfflineItemFilterImpl(OfflineItemFilterSource source) {
            super(source);
            onFilterChanged();
        }

        public void setFilters(Set<OfflineItem> items) {
            setFiltersSilently(items);
            onFilterChanged();
        }

        public void setFiltersSilently(Set<OfflineItem> items) {
            mFilteredItems.clear();
            mFilteredItems.addAll(items);
        }

        // OfflineItemFilter implementation.
        @Override
        protected boolean isFilteredOut(OfflineItem item) {
            if (mFilteredItems == null) return false;
            return mFilteredItems.contains(item);
        }
    }

    @Mock
    private OfflineItemFilterSource mSource;

    @Mock
    private OfflineItemFilterObserver mObserver;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    public void testBasicPassthrough() {
        // Test basic setup.
        OfflineItem item1 = new OfflineItem();
        OfflineItem item2 = new OfflineItem();
        OfflineItem item3 = new OfflineItem();
        Collection<OfflineItem> items = CollectionUtil.newHashSet(item1, item2, item3);
        when(mSource.getItems()).thenReturn(items);

        OfflineItemFilterImpl filter = new OfflineItemFilterImpl(mSource);
        filter.addObserver(mObserver);
        verify(mSource, times(1)).addObserver(filter);
        verify(mSource, times(1)).getItems();
        Assert.assertEquals(items, filter.getItems());

        // Test basic addition.
        OfflineItem item4 = new OfflineItem();
        OfflineItem item5 = new OfflineItem();
        Collection<OfflineItem> items2 = CollectionUtil.newHashSet(item4, item5);
        items.addAll(items2);

        filter.onItemsAdded(items2);
        verify(mObserver, times(1)).onItemsAdded(items2);
        Assert.assertEquals(items, filter.getItems());

        // Test basic removal.
        items2 = CollectionUtil.newHashSet(item3, item4);
        items.removeAll(items2);

        filter.onItemsRemoved(items2);
        verify(mObserver, times(1)).onItemsRemoved(items2);
        Assert.assertEquals(items, filter.getItems());

        // Test basic updating.
        OfflineItem newItem2 = new OfflineItem();
        items.remove(item2);
        items.add(newItem2);

        filter.onItemUpdated(item2, newItem2);
        verify(mObserver, times(1)).onItemUpdated(item2, newItem2);
        Assert.assertEquals(items, filter.getItems());
    }

    @Test
    public void testItemsAvailable() {
        when(mSource.areItemsAvailable()).thenReturn(false);

        OfflineItemFilterImpl filter = new OfflineItemFilterImpl(mSource);
        filter.addObserver(mObserver);
        verify(mSource, times(1)).addObserver(filter);
        verify(mSource, times(1)).getItems();

        Assert.assertFalse(filter.areItemsAvailable());

        when(mSource.areItemsAvailable()).thenReturn(true);
        filter.onItemsAvailable();
        verify(mObserver, times(1)).onItemsAvailable();
        Assert.assertTrue(filter.areItemsAvailable());
    }

    @Test
    public void testFiltering() {
        OfflineItem item1 = new OfflineItem();
        OfflineItem item2 = new OfflineItem();
        OfflineItem item3 = new OfflineItem();
        Collection<OfflineItem> sourceItems = CollectionUtil.newHashSet(item1, item2, item3);
        when(mSource.getItems()).thenReturn(sourceItems);

        OfflineItemFilterImpl filter = new OfflineItemFilterImpl(mSource);
        filter.addObserver(mObserver);
        verify(mSource, times(1)).addObserver(filter);
        verify(mSource, times(1)).getItems();
        Assert.assertEquals(sourceItems, filter.getItems());

        // Filter items.
        OfflineItem item4 = new OfflineItem();
        filter.setFilters(CollectionUtil.newHashSet(item1, item3, item4));
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item1, item3));
        Assert.assertEquals(CollectionUtil.newHashSet(item2), filter.getItems());

        // Update the filter.
        filter.setFilters(CollectionUtil.newHashSet(item1, item2, item4));
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item2));
        verify(mObserver, times(1)).onItemsAdded(CollectionUtil.newHashSet(item3));
        Assert.assertEquals(CollectionUtil.newHashSet(item3), filter.getItems());

        // Add a filtered item.
        sourceItems.add(item4);
        filter.onItemsAdded(CollectionUtil.newHashSet(item4));
        verify(mObserver, never()).onItemsAdded(CollectionUtil.newHashSet(item4));
        Assert.assertEquals(CollectionUtil.newHashSet(item3), filter.getItems());

        // Remove a filtered item.
        sourceItems.remove(item1);
        filter.onItemsRemoved(CollectionUtil.newHashSet(item1));
        verify(mObserver, never()).onItemsRemoved(CollectionUtil.newHashSet(item1));
        Assert.assertEquals(CollectionUtil.newHashSet(item3), filter.getItems());

        // Update a filtered item.
        OfflineItem newItem2 = new OfflineItem();
        sourceItems.remove(item2);
        sourceItems.add(newItem2);
        filter.setFiltersSilently(CollectionUtil.newHashSet(item1, newItem2, item4));
        filter.onItemUpdated(item2, newItem2);
        verify(mObserver, never()).onItemUpdated(item2, newItem2);
        Assert.assertEquals(CollectionUtil.newHashSet(item3), filter.getItems());

        // Add an unfiltered item.
        OfflineItem item5 = new OfflineItem();
        sourceItems.add(item5);
        filter.onItemsAdded(CollectionUtil.newHashSet(item5));
        verify(mObserver, times(1)).onItemsAdded(CollectionUtil.newHashSet(item5));
        Assert.assertEquals(CollectionUtil.newHashSet(item3, item5), filter.getItems());

        // Remove an unfiltered item.
        sourceItems.remove(item5);
        filter.onItemsRemoved(CollectionUtil.newHashSet(item5));
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item5));
        Assert.assertEquals(CollectionUtil.newHashSet(item3), filter.getItems());

        // Update an unfiltered item.
        OfflineItem newItem3 = new OfflineItem();
        sourceItems.remove(item3);
        sourceItems.add(newItem3);
        filter.onItemUpdated(item3, newItem3);
        verify(mObserver, times(1)).onItemUpdated(item3, newItem3);
        Assert.assertEquals(CollectionUtil.newHashSet(newItem3), filter.getItems());
    }

    @Test
    public void testUpdateFiltering() {
        OfflineItem item1 = new OfflineItem();
        OfflineItem item2 = new OfflineItem();
        Collection<OfflineItem> sourceItems = CollectionUtil.newHashSet(item1, item2);
        when(mSource.getItems()).thenReturn(sourceItems);

        OfflineItemFilterImpl filter = new OfflineItemFilterImpl(mSource);
        filter.addObserver(mObserver);
        verify(mSource, times(1)).addObserver(filter);
        verify(mSource, times(1)).getItems();
        Assert.assertEquals(sourceItems, filter.getItems());

        // Filter items.
        filter.setFilters(CollectionUtil.newHashSet(item2));
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item2));
        Assert.assertEquals(CollectionUtil.newHashSet(item1), filter.getItems());

        // Updated item goes from unfiltered to filtered.
        OfflineItem newItem1 = new OfflineItem();
        filter.setFiltersSilently(CollectionUtil.newHashSet(newItem1, item2));
        sourceItems.remove(item1);
        sourceItems.add(newItem1);
        filter.onItemUpdated(item1, newItem1);
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item1));
        Assert.assertTrue(filter.getItems().isEmpty());

        // Updated item goes from filtered to unfiltered.
        OfflineItem newItem2 = new OfflineItem();
        filter.setFiltersSilently(CollectionUtil.newHashSet(newItem1));
        sourceItems.remove(item2);
        sourceItems.add(newItem2);
        filter.onItemUpdated(item2, newItem2);
        verify(mObserver, times(1)).onItemsAdded(CollectionUtil.newHashSet(newItem2));
        Assert.assertEquals(CollectionUtil.newHashSet(newItem2), filter.getItems());
    }
}