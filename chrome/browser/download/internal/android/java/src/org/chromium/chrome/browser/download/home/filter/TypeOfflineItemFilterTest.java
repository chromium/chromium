// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

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
import org.chromium.components.offline_items_collection.OfflineItemFilter;

import java.util.Collection;

/** Unit tests for the TypeOfflineItemFilter class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TypeOfflineItemFilterTest {
    @Mock private OfflineItemFilterSource mSource;

    @Mock private OfflineItemFilterObserver mObserver;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    public void testTypeFiltering() {
        OfflineItem item1 = buildItem(OfflineItemFilter.PAGE, false);
        OfflineItem item2 = buildItem(OfflineItemFilter.VIDEO, false);
        OfflineItem item3 = buildItem(OfflineItemFilter.AUDIO, false);
        OfflineItem item4 = buildItem(OfflineItemFilter.IMAGE, false);
        OfflineItem item5 = buildItem(OfflineItemFilter.OTHER, false);
        OfflineItem item6 = buildItem(OfflineItemFilter.DOCUMENT, false);
        OfflineItem item7 = buildItem(OfflineItemFilter.PAGE, true);
        OfflineItem item8 = buildItem(OfflineItemFilter.VIDEO, true);
        Collection<OfflineItem> sourceItems =
                CollectionUtil.newHashSet(item1, item2, item3, item4, item5, item6, item7, item8);
        when(mSource.getItems()).thenReturn(sourceItems);

        TypeOfflineItemFilter filter = new TypeOfflineItemFilter(mSource);
        filter.addObserver(mObserver);
        Assert.assertEquals(
                CollectionUtil.newHashSet(item1, item2, item3, item4, item5, item6),
                filter.getItems());

        filter.onFilterSelected(Filters.FilterType.VIDEOS);
        verify(mObserver, times(1))
                .onItemsRemoved(CollectionUtil.newHashSet(item1, item3, item4, item5, item6));
        Assert.assertEquals(CollectionUtil.newHashSet(item2), filter.getItems());

        filter.onFilterSelected(Filters.FilterType.MUSIC);
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item2));
        verify(mObserver, times(1)).onItemsAdded(CollectionUtil.newHashSet(item3));
        Assert.assertEquals(CollectionUtil.newHashSet(item3), filter.getItems());

        filter.onFilterSelected(Filters.FilterType.IMAGES);
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item3));
        verify(mObserver, times(1)).onItemsAdded(CollectionUtil.newHashSet(item4));
        Assert.assertEquals(CollectionUtil.newHashSet(item4), filter.getItems());

        filter.onFilterSelected(Filters.FilterType.SITES);
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item4));
        verify(mObserver, times(1)).onItemsAdded(CollectionUtil.newHashSet(item1));
        Assert.assertEquals(CollectionUtil.newHashSet(item1), filter.getItems());

        filter.onFilterSelected(Filters.FilterType.OTHER);
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item1));
        verify(mObserver, times(1)).onItemsAdded(CollectionUtil.newHashSet(item5, item6));
        Assert.assertEquals(CollectionUtil.newHashSet(item5, item6), filter.getItems());

        filter.onFilterSelected(Filters.FilterType.PREFETCHED);
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item5, item6));
        verify(mObserver, times(1)).onItemsAdded(CollectionUtil.newHashSet(item7, item8));
        Assert.assertEquals(CollectionUtil.newHashSet(item7, item8), filter.getItems());

        filter.onFilterSelected(Filters.FilterType.NONE);
        verify(mObserver, times(1))
                .onItemsAdded(CollectionUtil.newHashSet(item1, item2, item3, item4, item5, item6));
        Assert.assertEquals(
                CollectionUtil.newHashSet(item1, item2, item3, item4, item5, item6),
                filter.getItems());
    }

    private static OfflineItem buildItem(@OfflineItemFilter int filter, boolean isSuggested) {
        OfflineItem item = new OfflineItem();
        item.filter = filter;
        item.isSuggested = isSuggested;
        return item;
    }
}
